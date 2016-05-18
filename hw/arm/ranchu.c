/*
 * ARM Android emulator 'ranchu' board.
 *
 * Copyright (c) 2014 Linaro Limited
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2 or later, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Emulate a virtual board for use as part of the Android emulator.
 * We create a device tree to pass to the kernel.
 * The board has a mixture of virtio devices and some Android-specific
 * devices inherited from the 32 bit 'goldfish' board.
 *
 * We only support 64-bit ARM CPUs.
 */

#include "hw/sysbus.h"
#include "hw/arm/arm.h"
#include "hw/arm/primecell.h"
#include "hw/devices.h"
#include "net/net.h"
#include "sysemu/device_tree.h"
#include "sysemu/sysemu.h"
#include "sysemu/kvm.h"
#include "hw/boards.h"
#include "exec/address-spaces.h"
#include "qemu/bitops.h"
#include "qemu/error-report.h"
#include "qemu/config-file.h"
#include "sysemu/char.h"
#include "monitor/monitor.h"
#include "hw/misc/android_pipe.h"

#ifdef USE_ANDROID_EMU
#include "android/android.h"
#else
#include "android-console.h"
#endif

/* Maximum number of emulators that can run at once (affects how
 * far through the TCP port space from 5554 we will scan to find
 * a pair of ports we can listen on)
 */
#define MAX_ANDROID_EMULATORS 64
#define ANDROID_CONSOLE_BASEPORT 5554

#define NUM_VIRTIO_TRANSPORTS 32

/* Number of external interrupt lines to configure the GIC with */
#define NUM_IRQS 128

#define GIC_FDT_IRQ_TYPE_SPI 0
#define GIC_FDT_IRQ_TYPE_PPI 1

#define GIC_FDT_IRQ_FLAGS_EDGE_LO_HI 1
#define GIC_FDT_IRQ_FLAGS_EDGE_HI_LO 2
#define GIC_FDT_IRQ_FLAGS_LEVEL_HI 4
#define GIC_FDT_IRQ_FLAGS_LEVEL_LO 8

#define GIC_FDT_IRQ_PPI_CPU_START 8
#define GIC_FDT_IRQ_PPI_CPU_WIDTH 8

enum {
    RANCHU_FLASH,
    RANCHU_MEM,
    RANCHU_CPUPERIPHS,
    RANCHU_GIC_DIST,
    RANCHU_GIC_CPU,
    RANCHU_UART,
    RANCHU_GF_FB,
    RANCHU_GF_BATTERY,
    RANCHU_GF_AUDIO,
    RANCHU_GF_EVDEV,
    RANCHU_ANDROID_PIPE,
    RANCHU_MMIO,
};

typedef struct MemMapEntry {
    hwaddr base;
    hwaddr size;
} MemMapEntry;

typedef struct VirtBoardInfo {
    struct arm_boot_info bootinfo;
    const char *cpu_model;
    const MemMapEntry *memmap;
    const int *irqmap;
    int smp_cpus;
    void *fdt;
    int fdt_size;
    uint32_t clock_phandle;
} VirtBoardInfo;

/* Addresses and sizes of our components.
 * 0..128MB is space for a flash device so we can run bootrom code such as UEFI.
 * 128MB..256MB is used for miscellaneous device I/O.
 * 256MB..1GB is reserved for possible future PCI support (ie where the
 * PCI memory window will go if we add a PCI host controller).
 * 1GB and up is RAM (which may happily spill over into the
 * high memory region beyond 4GB).
 * This represents a compromise between how much RAM can be given to
 * a 32 bit VM and leaving space for expansion and in particular for PCI.
 * Note that generally devices should be placed at multiples of 0x10000
 * to allow for the possibility of the guest using 64K pages.
 */
static const MemMapEntry memmap[] = {
    /* Space up to 0x8000000 is reserved for a boot ROM */
    [RANCHU_FLASH] = { 0, 0x8000000 },
    [RANCHU_CPUPERIPHS] = { 0x8000000, 0x20000 },
    /* GIC distributor and CPU interfaces sit inside the CPU peripheral space */
    [RANCHU_GIC_DIST] = { 0x8000000, 0x10000 },
    [RANCHU_GIC_CPU] = { 0x8010000, 0x10000 },
    [RANCHU_UART] = { 0x9000000, 0x1000 },
    [RANCHU_GF_FB] = { 0x9010000, 0x100 },
    [RANCHU_GF_BATTERY] = { 0x9020000, 0x1000 },
    [RANCHU_GF_AUDIO] = { 0x9030000, 0x100 },
    [RANCHU_GF_EVDEV] = { 0x9040000, 0x1000 },
    [RANCHU_MMIO] = { 0xa000000, 0x200 },
    [RANCHU_ANDROID_PIPE] = {0xa010000, 0x2000 },
    /* ...repeating for a total of NUM_VIRTIO_TRANSPORTS, each of that size */
    /* 0x10000000 .. 0x40000000 reserved for PCI */
    [RANCHU_MEM] = { 0x40000000, 30ULL * 1024 * 1024 * 1024 },
};

static const int irqmap[] = {
    [RANCHU_UART] = 1,
    [RANCHU_GF_FB] = 2,
    [RANCHU_GF_BATTERY] = 3,
    [RANCHU_GF_AUDIO] = 4,
    [RANCHU_GF_EVDEV] = 5,
    [RANCHU_ANDROID_PIPE] = 6,
    [RANCHU_MMIO] = 16, /* ...to 16 + NUM_VIRTIO_TRANSPORTS - 1 */
};

static void create_fdt(VirtBoardInfo *vbi)
{
    void *fdt = create_device_tree(&vbi->fdt_size);

    if (!fdt) {
        error_report("create_device_tree() failed");
        exit(1);
    }

    vbi->fdt = fdt;

    /* Header */
    qemu_fdt_setprop_string(fdt, "/", "compatible", "ranchu");
    qemu_fdt_setprop_cell(fdt, "/", "#address-cells", 0x2);
    qemu_fdt_setprop_cell(fdt, "/", "#size-cells", 0x2);

    /* Firmware node */
    qemu_fdt_add_subnode(fdt, "/firmware");
    qemu_fdt_add_subnode(fdt, "/firmware/android");
    qemu_fdt_setprop_string(fdt, "/firmware/android", "compatible", "android,firmware");
    qemu_fdt_setprop_string(fdt, "/firmware/android", "hardware", "ranchu");

    /*
     * /chosen and /memory nodes must exist for load_dtb
     * to fill in necessary properties later
     */
    qemu_fdt_add_subnode(fdt, "/chosen");
    qemu_fdt_add_subnode(fdt, "/memory");
    qemu_fdt_setprop_string(fdt, "/memory", "device_type", "memory");

    /* Clock node, for the benefit of the UART. The kernel device tree
     * binding documentation claims the PL011 node clock properties are
     * optional but in practice if you omit them the kernel refuses to
     * probe for the device.
     */
    vbi->clock_phandle = qemu_fdt_alloc_phandle(fdt);
    qemu_fdt_add_subnode(fdt, "/apb-pclk");
    qemu_fdt_setprop_string(fdt, "/apb-pclk", "compatible", "fixed-clock");
    qemu_fdt_setprop_cell(fdt, "/apb-pclk", "#clock-cells", 0x0);
    qemu_fdt_setprop_cell(fdt, "/apb-pclk", "clock-frequency", 24000000);
    qemu_fdt_setprop_string(fdt, "/apb-pclk", "clock-output-names",
                                "clk24mhz");
    qemu_fdt_setprop_cell(fdt, "/apb-pclk", "phandle", vbi->clock_phandle);

    /* No PSCI for TCG yet */
    if (kvm_enabled()) {
        uint32_t cpu_suspend_fn;
        uint32_t cpu_off_fn;
        uint32_t cpu_on_fn;
        uint32_t migrate_fn;
        ARMCPU *armcpu = ARM_CPU(qemu_get_cpu(0));

        qemu_fdt_add_subnode(fdt, "/psci");
        if (armcpu->psci_version == 2) {
            const char comp[] = "arm,psci-0.2\0arm,psci";
            qemu_fdt_setprop(fdt, "/psci", "compatible", comp, sizeof(comp));

            cpu_off_fn = QEMU_PSCI_0_2_FN_CPU_OFF;
            if (arm_feature(&armcpu->env, ARM_FEATURE_AARCH64)) {
                cpu_suspend_fn = QEMU_PSCI_0_2_FN64_CPU_SUSPEND;
                cpu_on_fn = QEMU_PSCI_0_2_FN64_CPU_ON;
                migrate_fn = QEMU_PSCI_0_2_FN64_MIGRATE;
            } else {
                cpu_suspend_fn = QEMU_PSCI_0_2_FN_CPU_SUSPEND;
                cpu_on_fn = QEMU_PSCI_0_2_FN_CPU_ON;
                migrate_fn = QEMU_PSCI_0_2_FN_MIGRATE;
            }
        } else {
            qemu_fdt_setprop_string(fdt, "/psci", "compatible", "arm,psci");

            cpu_suspend_fn = QEMU_PSCI_0_1_FN_CPU_SUSPEND;
            cpu_off_fn = QEMU_PSCI_0_1_FN_CPU_OFF;
            cpu_on_fn = QEMU_PSCI_0_1_FN_CPU_ON;
            migrate_fn = QEMU_PSCI_0_1_FN_MIGRATE;
        }

        qemu_fdt_setprop_string(fdt, "/psci", "method", "hvc");

        qemu_fdt_setprop_cell(fdt, "/psci", "cpu_suspend", cpu_suspend_fn);
        qemu_fdt_setprop_cell(fdt, "/psci", "cpu_off", cpu_off_fn);
        qemu_fdt_setprop_cell(fdt, "/psci", "cpu_on", cpu_on_fn);
        qemu_fdt_setprop_cell(fdt, "/psci", "migrate", migrate_fn);
    }
}

static void fdt_add_timer_nodes(const VirtBoardInfo *vbi)
{
    /* Note that on A15 h/w these interrupts are level-triggered,
     * but for the GIC implementation provided by both QEMU and KVM
     * they are edge-triggered.
     */
    uint32_t irqflags = GIC_FDT_IRQ_FLAGS_EDGE_LO_HI;

    irqflags = deposit32(irqflags, GIC_FDT_IRQ_PPI_CPU_START,
                         GIC_FDT_IRQ_PPI_CPU_WIDTH, (1 << vbi->smp_cpus) - 1);

    qemu_fdt_add_subnode(vbi->fdt, "/timer");
    qemu_fdt_setprop_string(vbi->fdt, "/timer",
                                "compatible", "arm,armv7-timer");
    qemu_fdt_setprop_cells(vbi->fdt, "/timer", "interrupts",
                               GIC_FDT_IRQ_TYPE_PPI, 13, irqflags,
                               GIC_FDT_IRQ_TYPE_PPI, 14, irqflags,
                               GIC_FDT_IRQ_TYPE_PPI, 11, irqflags,
                               GIC_FDT_IRQ_TYPE_PPI, 10, irqflags);
}

static void fdt_add_cpu_nodes(const VirtBoardInfo *vbi)
{
    int cpu;

    qemu_fdt_add_subnode(vbi->fdt, "/cpus");
    qemu_fdt_setprop_cell(vbi->fdt, "/cpus", "#address-cells", 0x1);
    qemu_fdt_setprop_cell(vbi->fdt, "/cpus", "#size-cells", 0x0);

    for (cpu = vbi->smp_cpus - 1; cpu >= 0; cpu--) {
        char *nodename = g_strdup_printf("/cpus/cpu@%d", cpu);
        ARMCPU *armcpu = ARM_CPU(qemu_get_cpu(cpu));

        qemu_fdt_add_subnode(vbi->fdt, nodename);
        qemu_fdt_setprop_string(vbi->fdt, nodename, "device_type", "cpu");
        qemu_fdt_setprop_string(vbi->fdt, nodename, "compatible",
                                    armcpu->dtb_compatible);

        if (vbi->smp_cpus > 1) {
            qemu_fdt_setprop_string(vbi->fdt, nodename,
                                        "enable-method", "psci");
        }

        qemu_fdt_setprop_cell(vbi->fdt, nodename, "reg", cpu);
        g_free(nodename);
    }
}

static void fdt_add_gic_node(const VirtBoardInfo *vbi)
{
    uint32_t gic_phandle;

    gic_phandle = qemu_fdt_alloc_phandle(vbi->fdt);
    qemu_fdt_setprop_cell(vbi->fdt, "/", "interrupt-parent", gic_phandle);

    qemu_fdt_add_subnode(vbi->fdt, "/intc");
    /* 'cortex-a15-gic' means 'GIC v2' */
    qemu_fdt_setprop_string(vbi->fdt, "/intc", "compatible",
                            "arm,cortex-a15-gic");
    qemu_fdt_setprop_cell(vbi->fdt, "/intc", "#interrupt-cells", 3);
    qemu_fdt_setprop(vbi->fdt, "/intc", "interrupt-controller", NULL, 0);
    qemu_fdt_setprop_sized_cells(vbi->fdt, "/intc", "reg",
                                     2, memmap[RANCHU_GIC_DIST].base,
                                     2, memmap[RANCHU_GIC_DIST].size,
                                     2, memmap[RANCHU_GIC_CPU].base,
                                     2, memmap[RANCHU_GIC_CPU].size);
    qemu_fdt_setprop_cell(vbi->fdt, "/intc", "phandle", gic_phandle);
}

static void create_gic(const VirtBoardInfo *vbi, qemu_irq *pic)
{
    /* We create a standalone GIC v2 */
    DeviceState *gicdev;
    SysBusDevice *gicbusdev;
    const char *gictype = "arm_gic";
    int i;

    if (kvm_irqchip_in_kernel()) {
        gictype = "kvm-arm-gic";
    }

    gicdev = qdev_create(NULL, gictype);
    qdev_prop_set_uint32(gicdev, "revision", 2);
    qdev_prop_set_uint32(gicdev, "num-cpu", smp_cpus);
    /* Note that the num-irq property counts both internal and external
     * interrupts; there are always 32 of the former (mandated by GIC spec).
     */
    qdev_prop_set_uint32(gicdev, "num-irq", NUM_IRQS + 32);
    qdev_init_nofail(gicdev);
    gicbusdev = SYS_BUS_DEVICE(gicdev);
    sysbus_mmio_map(gicbusdev, 0, memmap[RANCHU_GIC_DIST].base);
    sysbus_mmio_map(gicbusdev, 1, memmap[RANCHU_GIC_CPU].base);

    /* Wire the outputs from each CPU's generic timer to the
     * appropriate GIC PPI inputs, and the GIC's IRQ output to
     * the CPU's IRQ input.
     */
    for (i = 0; i < smp_cpus; i++) {
        DeviceState *cpudev = DEVICE(qemu_get_cpu(i));
        int ppibase = NUM_IRQS + i * 32;
        /* physical timer; we wire it up to the non-secure timer's ID,
         * since a real A15 always has TrustZone but QEMU doesn't.
         */
        qdev_connect_gpio_out(cpudev, 0,
                              qdev_get_gpio_in(gicdev, ppibase + 30));
        /* virtual timer */
        qdev_connect_gpio_out(cpudev, 1,
                              qdev_get_gpio_in(gicdev, ppibase + 27));

        sysbus_connect_irq(gicbusdev, i, qdev_get_gpio_in(cpudev, ARM_CPU_IRQ));
    }

    for (i = 0; i < NUM_IRQS; i++) {
        pic[i] = qdev_get_gpio_in(gicdev, i);
    }

    fdt_add_gic_node(vbi);
}

/**
 * create_simple_device:
 * @vbi: VirtBoardInfo struct
 * @pic: interrupt array
 * @devid: the RANCHU_* index for this device
 * @sysbus_name: QEMU's name for the device
 * @compat: one or more NUL-separated DTB compat strings
 * @num_compat_strings: number of NUL-separated strings in @compat
 * @clocks: zero or more NUL-separated clock names
 * @num_clocks: number of NUL-separated clock names in @clocks
 *
 * Create a simple device with one interrupt and an uncomplicated
 * device tree node (one reg tuple, one interrupt, optional clocks).
 */
static void create_simple_device(const VirtBoardInfo *vbi, qemu_irq *pic,
                                 int devid, const char *sysbus_name,
                                 const char *compat, int num_compat_strings,
                                 const char *clocks, int num_clocks)
{
    int irq = irqmap[devid];
    hwaddr base = memmap[devid].base;
    hwaddr size = memmap[devid].size;
    char *nodename;
    int i;
    int compat_sz = 0;
    int clocks_sz = 0;

    for (i = 0; i < num_compat_strings; i++) {
        compat_sz += strlen(compat + compat_sz) + 1;
    }

    for (i = 0; i < num_clocks; i++) {
        clocks_sz += strlen(clocks + clocks_sz) + 1;
    }

    sysbus_create_simple(sysbus_name, base, pic[irq]);

    nodename = g_strdup_printf("/%s@%" PRIx64, sysbus_name, base);
    qemu_fdt_add_subnode(vbi->fdt, nodename);
    qemu_fdt_setprop(vbi->fdt, nodename, "compatible", compat, compat_sz);
    qemu_fdt_setprop_sized_cells(vbi->fdt, nodename, "reg", 2, base, 2, size);
    if (irq) {
        qemu_fdt_setprop_cells(vbi->fdt, nodename, "interrupts",
                               GIC_FDT_IRQ_TYPE_SPI, irq,
                               GIC_FDT_IRQ_FLAGS_LEVEL_HI);
    }
    if (num_clocks) {
        qemu_fdt_setprop_cells(vbi->fdt, nodename, "clocks",
                               vbi->clock_phandle, vbi->clock_phandle);
        qemu_fdt_setprop(vbi->fdt, nodename, "clock-names",
                         clocks, clocks_sz);
    }
    g_free(nodename);
}

static void create_virtio_devices(const VirtBoardInfo *vbi, qemu_irq *pic)
{
    int i;
    hwaddr size = memmap[RANCHU_MMIO].size;

    /* Note that we have to create the transports in forwards order
     * so that command line devices are inserted lowest address first,
     * and then add dtb nodes in reverse order so that they appear in
     * the finished device tree lowest address first.
     */
    for (i = 0; i < NUM_VIRTIO_TRANSPORTS; i++) {
        int irq = irqmap[RANCHU_MMIO] + i;
        hwaddr base = memmap[RANCHU_MMIO].base + i * size;

        sysbus_create_simple("virtio-mmio", base, pic[irq]);
    }

    for (i = NUM_VIRTIO_TRANSPORTS - 1; i >= 0; i--) {
        char *nodename;
        int irq = irqmap[RANCHU_MMIO] + i;
        hwaddr base = memmap[RANCHU_MMIO].base + i * size;

        nodename = g_strdup_printf("/virtio_mmio@%" PRIx64, base);
        qemu_fdt_add_subnode(vbi->fdt, nodename);
        qemu_fdt_setprop_string(vbi->fdt, nodename,
                                "compatible", "virtio,mmio");
        qemu_fdt_setprop_sized_cells(vbi->fdt, nodename, "reg",
                                     2, base, 2, size);
        qemu_fdt_setprop_cells(vbi->fdt, nodename, "interrupts",
                               GIC_FDT_IRQ_TYPE_SPI, irq,
                               GIC_FDT_IRQ_FLAGS_EDGE_LO_HI);
        g_free(nodename);
    }
}

static void *ranchu_dtb(const struct arm_boot_info *binfo, int *fdt_size)
{
    const VirtBoardInfo *board = (const VirtBoardInfo *)binfo;

    *fdt_size = board->fdt_size;
    return board->fdt;
}

static CharDriverState *try_to_create_console_chardev(int portno)
{
    /* Try to create the chardev for the Android console on the specified port.
     * This is equivalent to the command line options
     *  -chardev socket,id=monitor,host=127.0.0.1,port=NNN,server,nowait,telnet

     *  -mon chardev=monitor,mode=android-console
     * Return true on success, false on failure (presumably port-in-use).
     */
    Error *err = NULL;
    CharDriverState *chr;
    QemuOpts *opts;
    const char *chardev_opts =
        "socket,id=private-chardev-for-android-monitor,"
        "host=127.0.0.1,server,nowait,telnet";

    opts = qemu_opts_parse(qemu_find_opts("chardev"), chardev_opts, 1);
    assert(opts);
    qemu_opt_set_number(opts, "port", portno);
    chr = qemu_chr_new_from_opts(opts, NULL, &err);
    if (err) {
        /* Assume this was port-in-use */
        qemu_opts_del(opts);
        error_free(err);
        return NULL;
    }

    qemu_chr_fe_claim_no_fail(chr);
    return chr;
}

static void initialize_console_and_adb(VirtBoardInfo *vbi)
{
    /* Initialize the console and ADB, which must listen on two
     * consecutive TCP ports starting from 5555 and working up until
     * we manage to open both connections.
     */
    int baseport = (android_base_port > ANDROID_CONSOLE_BASEPORT) ?
        android_base_port : ANDROID_CONSOLE_BASEPORT;

    int tries = MAX_ANDROID_EMULATORS;
    CharDriverState *chr;

    for (; tries > 0; tries--, baseport += 2) {
        chr = try_to_create_console_chardev(baseport);
        if (!chr) {
            continue;
        }

        if (!qemu2_adb_server_init(baseport + 1)) {
            qemu_chr_delete(chr);
            chr = NULL;
            continue;
        }

        /* Confirmed we have both ports, now we can create the console itself.
         * This is equivalent to
         * "-mon chardev=private-chardev,mode=android-console"
         */
        monitor_init(chr, MONITOR_ANDROID_CONSOLE | MONITOR_USE_READLINE);
        printf("console on port %d, ADB on port %d\n", baseport, baseport + 1);
        android_base_port = baseport;
        return;
    }
    error_report("it seems too many emulator instances are running "
                 "on this machine. Aborting\n");
    exit(1);
}

static void ranchu_init(MachineState *machine)
{
    qemu_irq pic[NUM_IRQS];
    MemoryRegion *sysmem = get_system_memory();
    int n;
    MemoryRegion *ram = g_new(MemoryRegion, 1);
    const char *cpu_model = machine->cpu_model;
    VirtBoardInfo *vbi;

    if (!cpu_model) {
        cpu_model = "cortex-a57";
    }

    vbi = g_new0(VirtBoardInfo, 1);

    vbi->smp_cpus = smp_cpus;

    if (machine->ram_size > memmap[RANCHU_MEM].size) {
        error_report("ranchu: cannot model more than 30GB RAM");
        exit(1);
    }

    create_fdt(vbi);
    fdt_add_timer_nodes(vbi);

    for (n = 0; n < smp_cpus; n++) {
        ObjectClass *oc = cpu_class_by_name(TYPE_ARM_CPU, cpu_model);
        Object *cpuobj;

        if (!oc) {
            fprintf(stderr, "Unable to find CPU definition\n");
            exit(1);
        }
        cpuobj = object_new(object_class_get_name(oc));

        /* Secondary CPUs start in PSCI powered-down state */
        if (n > 0) {
            object_property_set_bool(cpuobj, true, "start-powered-off", NULL);
        }

        if (object_property_find(cpuobj, "reset-cbar", NULL)) {
            object_property_set_int(cpuobj, memmap[RANCHU_CPUPERIPHS].base,
                                    "reset-cbar", &error_abort);
        }

        object_property_set_bool(cpuobj, true, "realized", NULL);
    }
    fdt_add_cpu_nodes(vbi);

    memory_region_init_ram(ram, NULL, "ranchu.ram", machine->ram_size,
                           &error_abort);
    vmstate_register_ram_global(ram);
    memory_region_add_subregion(sysmem, memmap[RANCHU_MEM].base, ram);

    create_gic(vbi, pic);
    create_simple_device(vbi, pic, RANCHU_UART, "pl011",
                         "arm,pl011\0arm,primecell", 2, "uartclk\0apb_pclk", 2);
    create_simple_device(vbi, pic, RANCHU_GF_FB, "goldfish_fb",
                         "generic,goldfish-fb", 1, 0, 0);
    create_simple_device(vbi, pic, RANCHU_GF_BATTERY, "goldfish_battery",
                         "generic,goldfish-battery", 1, 0, 0);
    create_simple_device(vbi, pic, RANCHU_GF_AUDIO, "goldfish_audio",
                         "generic,goldfish-audio", 1, 0, 0);
    create_simple_device(vbi, pic, RANCHU_GF_EVDEV, "goldfish-events",
                         "generic,goldfish-events-keypad", 1, 0, 0);
    create_simple_device(vbi, pic, RANCHU_ANDROID_PIPE, "android_pipe",
                         "generic,android-pipe", 1, 0, 0);

    /* Create mmio transports, so the user can create virtio backends
     * (which will be automatically plugged in to the transports). If
     * no backend is created the transport will just sit harmlessly idle.
     */
    create_virtio_devices(vbi, pic);

    /* Initialize the Android console and adb connection
     * (must be done after the pipe has been realized).
     */
    initialize_console_and_adb(vbi);

    vbi->bootinfo.ram_size = machine->ram_size;
    vbi->bootinfo.kernel_filename = machine->kernel_filename;
    vbi->bootinfo.kernel_cmdline = machine->kernel_cmdline;
    vbi->bootinfo.initrd_filename = machine->initrd_filename;
    vbi->bootinfo.nb_cpus = smp_cpus;
    vbi->bootinfo.board_id = -1;
    vbi->bootinfo.loader_start = memmap[RANCHU_MEM].base;
    vbi->bootinfo.get_dtb = ranchu_dtb;
    arm_load_kernel(ARM_CPU(first_cpu), &vbi->bootinfo);
}

static QEMUMachine ranchu_machine = {
    .name = "ranchu",
    .desc = "Ranchu Virtual Machine for Android Emulator",
    .init = ranchu_init,
    .max_cpus = 1,
};

static void ranchu_machine_init(void)
{
    qemu_register_machine(&ranchu_machine);
}

machine_init(ranchu_machine_init);
