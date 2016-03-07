/*
 * QEMU System Emulator
 *
 * Copyright (c) 2003-2008 Fabrice Bellard
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>
#include <errno.h>
#include <sys/time.h>

#include "config-host.h"

#ifdef CONFIG_SECCOMP
#include "sysemu/seccomp.h"
#endif

#if defined(CONFIG_VDE)
#include <libvdeplug.h>
#endif

#include <glib.h>

#include "qemu/sockets.h"
#include "hw/hw.h"
#include "hw/boards.h"
#include "sysemu/accel.h"
#include "hw/usb.h"
#include "hw/i386/pc.h"
#include "hw/isa/isa.h"
#include "hw/bt.h"
#include "sysemu/watchdog.h"
#include "hw/i386/smbios.h"
#include "hw/xen/xen.h"
#include "hw/qdev.h"
#include "hw/loader.h"
#include "monitor/qdev.h"
#include "sysemu/bt.h"
#include "net/net.h"
#include "net/slirp.h"
#include "monitor/monitor.h"
#include "ui/console.h"
#include "sysemu/sysemu.h"
#include "exec/gdbstub.h"
#include "qemu/timer.h"
#include "sysemu/char.h"
#include "qemu/bitmap.h"
#include "sysemu/blockdev.h"
#include "hw/block/block.h"
#include "migration/block.h"
#include "sysemu/tpm.h"
#include "sysemu/dma.h"
#include "audio/audio.h"
#include "migration/migration.h"
#include "sysemu/kvm.h"
#include "sysemu/hax.h"
#include "qapi/qmp/qjson.h"
#include "qemu/option.h"
#include "qemu/config-file.h"
#include "qemu-options.h"
#include "qmp-commands.h"
#include "qemu/main-loop.h"
#ifdef CONFIG_VIRTFS
#include "fsdev/qemu-fsdev.h"
#endif
#include "sysemu/qtest.h"

#include "disas/disas.h"


#include "slirp/libslirp.h"

#include "trace.h"
#include "trace/control.h"
#include "qemu/queue.h"
#include "sysemu/cpus.h"
#include "sysemu/arch_init.h"
#include "qemu/osdep.h"

#include "ui/qemu-spice.h"
#include "qapi/string-input-visitor.h"
#include "qapi/opts-visitor.h"
#include "qom/object_interfaces.h"
#include "qapi-event.h"

#ifdef CONFIG_ANDROID

#ifdef USE_ANDROID_EMU
#include "config.h"

#include "android/boot-properties.h"
#include "android/error-messages.h"
#include "android/crashreport/crash-handler.h"
#include "android/emulation/bufprint_config_dirs.h"
#include "android/metrics/metrics_reporter.h"
#include "android/metrics/studio-helper.h"
#include "android/update-check/update_check.h"
#include "android/utils/async.h"
#include "android/utils/debug.h"
#include "android/utils/path.h"
#include "android/utils/property_file.h"
#include "android/utils/lineinput.h"
#include "android/utils/bufprint.h"
#include "android/utils/filelock.h"
#include "android/utils/ini.h"
#include "android/utils/tempfile.h"
#include "android/skin/winsys.h"
#include "android/main-common.h"
#include "android/multitouch-port.h"
#include "android/opengl/emugl_config.h"
#include "android/ui-emu-agent.h"
#include "android/globals.h"
#include "android/help.h"
#include "android-qemu2-glue/looper-qemu.h"
#include "android/gps.h"
#include "android/telephony/modem_driver.h"
#include "android/hw-control.h"
#include "android/hw-kmsg.h"
#include "android/hw-qemud.h"
#include "android/utils/socket_drainer.h"
#include "android/wear-agent/android_wear_agent.h"
#include "android-qemu2-glue/android_qemud.h"
#include "android-qemu2-glue/qemu-control-impl.h"
#include "android-qemu2-glue/qemu-setup.h"
#include "android/snapshot.h"
#include "android/snaphost-android.h"
#include "android/android.h"
#include "android/camera/camera-service.h"
#include "android/opengles.h"
#include "android/version.h"

#define QEMU_CORE_VERSION "qemu2 " QEMU_VERSION

int android_display_width  = 640;
int android_display_height = 480;
int android_display_bpp    = 32;
int android_display_use_host_gpu = 0;

/////////////////////////////////////////////////////////////

#endif  /* USE_ANDROID_EMU */

#define  LCD_DENSITY_LDPI      120
#define  LCD_DENSITY_MDPI      160
#define  LCD_DENSITY_TVDPI     213
#define  LCD_DENSITY_HDPI      240
#define  LCD_DENSITY_280DPI    280
#define  LCD_DENSITY_XHDPI     320
#define  LCD_DENSITY_360DPI    360
#define  LCD_DENSITY_400DPI    400
#define  LCD_DENSITY_420DPI    420
#define  LCD_DENSITY_XXHDPI    480
#define  LCD_DENSITY_560DPI    560
#define  LCD_DENSITY_XXXHDPI   640
#endif // CONFIG_ANDROID

#define DEFAULT_RAM_SIZE 128

#define MAX_VIRTIO_CONSOLES 1
#define MAX_SCLP_CONSOLES 1


static const char *data_dir[16];
static int data_dir_idx;
const char *bios_name = NULL;
enum vga_retrace_method vga_retrace_method = VGA_RETRACE_DUMB;
DisplayType display_type = DT_DEFAULT;
static int display_remote;
const char* keyboard_layout = NULL;
ram_addr_t ram_size;
const char *mem_path = NULL;
int mem_prealloc = 0; /* force preallocation of physical target memory */
bool enable_mlock = false;
int nb_nics;
NICInfo nd_table[MAX_NICS];
int autostart;
static int rtc_utc = 1;
static int rtc_date_offset = -1; /* -1 means no change */
QEMUClockType rtc_clock;
int vga_interface_type = VGA_NONE;
static int full_screen = 0;
static int no_frame = 0;
int no_quit = 0;
#ifdef CONFIG_GTK
static bool grab_on_hover;
#endif
CharDriverState *serial_hds[MAX_SERIAL_PORTS];
CharDriverState *parallel_hds[MAX_PARALLEL_PORTS];
CharDriverState *virtcon_hds[MAX_VIRTIO_CONSOLES];
CharDriverState *sclp_hds[MAX_SCLP_CONSOLES];
int win2k_install_hack = 0;
int singlestep = 0;
int smp_cpus = 1;
int max_cpus = 0;
int smp_cores = 1;
int smp_threads = 1;
#ifdef CONFIG_VNC
const char *vnc_display;
#endif
int acpi_enabled = 1;
int no_hpet = 0;
int fd_bootchk = 1;
static int no_reboot;
int no_shutdown = 0;
int cursor_hide = 1;
int graphic_rotate = 0;
#ifdef CONFIG_ANDROID
/* -netspeed option value. */
char* android_op_netspeed = NULL;
char* android_op_netdelay = NULL;
int android_op_netfast = 0;
char* android_op_dns_server = NULL;
int lcd_density = LCD_DENSITY_MDPI;
char* additional_kernel_params = NULL;
#ifdef USE_ANDROID_EMU
extern char* op_http_proxy;
static const char* android_hw_file = NULL;
#endif  // USE_ANDROID_EMU
#endif  // CONFIG_ANDROID
const char *watchdog;
QEMUOptionRom option_rom[MAX_OPTION_ROMS];
int nb_option_roms;
int semihosting_enabled = 0;
int old_param = 0;
const char *qemu_name;
int alt_grab = 0;
int ctrl_grab = 0;
unsigned int nb_prom_envs = 0;
const char *prom_envs[MAX_PROM_ENVS];
int boot_menu;
bool boot_strict;
uint8_t *boot_splash_filedata;
size_t boot_splash_filedata_size;
uint8_t qemu_extra_params_fw[2];

int icount_align_option;

int nb_numa_nodes;
int max_numa_nodeid;
NodeInfo numa_info[MAX_NODES];

/* The bytes in qemu_uuid[] are in the order specified by RFC4122, _not_ in the
 * little-endian "wire format" described in the SMBIOS 2.6 specification.
 */
uint8_t qemu_uuid[16];
bool qemu_uuid_set;

static QEMUBootSetHandler *boot_set_handler;
static void *boot_set_opaque;

static NotifierList exit_notifiers =
    NOTIFIER_LIST_INITIALIZER(exit_notifiers);

static NotifierList machine_init_done_notifiers =
    NOTIFIER_LIST_INITIALIZER(machine_init_done_notifiers);

bool xen_allowed;
uint32_t xen_domid;
enum xen_mode xen_mode = XEN_EMULATE;

static int has_defaults = 1;
static int default_serial = 1;
static int default_parallel = 1;
static int default_virtcon = 1;
static int default_sclp = 1;
static int default_monitor = 1;
static int default_floppy = 1;
static int default_cdrom = 1;
static int default_sdcard = 1;
static int default_vga = 1;

static struct {
    const char *driver;
    int *flag;
} default_list[] = {
    { .driver = "isa-serial",           .flag = &default_serial    },
    { .driver = "isa-parallel",         .flag = &default_parallel  },
    { .driver = "isa-fdc",              .flag = &default_floppy    },
    { .driver = "ide-cd",               .flag = &default_cdrom     },
    { .driver = "ide-hd",               .flag = &default_cdrom     },
    { .driver = "ide-drive",            .flag = &default_cdrom     },
    { .driver = "scsi-cd",              .flag = &default_cdrom     },
    { .driver = "virtio-serial-pci",    .flag = &default_virtcon   },
    { .driver = "virtio-serial-s390",   .flag = &default_virtcon   },
    { .driver = "virtio-serial",        .flag = &default_virtcon   },
    { .driver = "VGA",                  .flag = &default_vga       },
    { .driver = "isa-vga",              .flag = &default_vga       },
    { .driver = "cirrus-vga",           .flag = &default_vga       },
    { .driver = "isa-cirrus-vga",       .flag = &default_vga       },
    { .driver = "vmware-svga",          .flag = &default_vga       },
    { .driver = "qxl-vga",              .flag = &default_vga       },
};

static QemuOptsList qemu_rtc_opts = {
    .name = "rtc",
    .head = QTAILQ_HEAD_INITIALIZER(qemu_rtc_opts.head),
    .desc = {
        {
            .name = "base",
            .type = QEMU_OPT_STRING,
        },{
            .name = "clock",
            .type = QEMU_OPT_STRING,
        },{
            .name = "driftfix",
            .type = QEMU_OPT_STRING,
        },
        { /* end of list */ }
    },
};

static QemuOptsList qemu_sandbox_opts = {
    .name = "sandbox",
    .implied_opt_name = "enable",
    .head = QTAILQ_HEAD_INITIALIZER(qemu_sandbox_opts.head),
    .desc = {
        {
            .name = "enable",
            .type = QEMU_OPT_BOOL,
        },
        { /* end of list */ }
    },
};

static QemuOptsList qemu_trace_opts = {
    .name = "trace",
    .implied_opt_name = "trace",
    .head = QTAILQ_HEAD_INITIALIZER(qemu_trace_opts.head),
    .desc = {
        {
            .name = "events",
            .type = QEMU_OPT_STRING,
        },{
            .name = "file",
            .type = QEMU_OPT_STRING,
        },
        { /* end of list */ }
    },
};

static QemuOptsList qemu_option_rom_opts = {
    .name = "option-rom",
    .implied_opt_name = "romfile",
    .head = QTAILQ_HEAD_INITIALIZER(qemu_option_rom_opts.head),
    .desc = {
        {
            .name = "bootindex",
            .type = QEMU_OPT_NUMBER,
        }, {
            .name = "romfile",
            .type = QEMU_OPT_STRING,
        },
        { /* end of list */ }
    },
};

static QemuOptsList qemu_machine_opts = {
    .name = "machine",
    .implied_opt_name = "type",
    .merge_lists = true,
    .head = QTAILQ_HEAD_INITIALIZER(qemu_machine_opts.head),
    .desc = {
        {
            .name = "type",
            .type = QEMU_OPT_STRING,
            .help = "emulated machine"
        }, {
            .name = "accel",
            .type = QEMU_OPT_STRING,
            .help = "accelerator list",
        }, {
            .name = "kernel_irqchip",
            .type = QEMU_OPT_BOOL,
            .help = "use KVM in-kernel irqchip",
        }, {
            .name = "kvm_shadow_mem",
            .type = QEMU_OPT_SIZE,
            .help = "KVM shadow MMU size",
        }, {
            .name = "kernel",
            .type = QEMU_OPT_STRING,
            .help = "Linux kernel image file",
        }, {
            .name = "initrd",
            .type = QEMU_OPT_STRING,
            .help = "Linux initial ramdisk file",
        }, {
            .name = "append",
            .type = QEMU_OPT_STRING,
            .help = "Linux kernel command line",
        }, {
            .name = "dtb",
            .type = QEMU_OPT_STRING,
            .help = "Linux kernel device tree file",
        }, {
            .name = "dumpdtb",
            .type = QEMU_OPT_STRING,
            .help = "Dump current dtb to a file and quit",
        }, {
            .name = "phandle_start",
            .type = QEMU_OPT_NUMBER,
            .help = "The first phandle ID we may generate dynamically",
        }, {
            .name = "dt_compatible",
            .type = QEMU_OPT_STRING,
            .help = "Overrides the \"compatible\" property of the dt root node",
        }, {
            .name = "dump-guest-core",
            .type = QEMU_OPT_BOOL,
            .help = "Include guest memory in  a core dump",
        }, {
            .name = "mem-merge",
            .type = QEMU_OPT_BOOL,
            .help = "enable/disable memory merge support",
        },{
            .name = "usb",
            .type = QEMU_OPT_BOOL,
            .help = "Set on/off to enable/disable usb",
        },{
            .name = "firmware",
            .type = QEMU_OPT_STRING,
            .help = "firmware image",
        },{
            .name = "kvm-type",
            .type = QEMU_OPT_STRING,
            .help = "Specifies the KVM virtualization mode (HV, PR)",
        },{
            .name = PC_MACHINE_MAX_RAM_BELOW_4G,
            .type = QEMU_OPT_SIZE,
            .help = "maximum ram below the 4G boundary (32bit boundary)",
        }, {
            .name = PC_MACHINE_VMPORT,
            .type = QEMU_OPT_STRING,
            .help = "Enable vmport (pc & q35)",
        },{
            .name = "iommu",
            .type = QEMU_OPT_BOOL,
            .help = "Set on/off to enable/disable Intel IOMMU (VT-d)",
        },
        { /* End of list */ }
    },
};

static QemuOptsList qemu_boot_opts = {
    .name = "boot-opts",
    .implied_opt_name = "order",
    .merge_lists = true,
    .head = QTAILQ_HEAD_INITIALIZER(qemu_boot_opts.head),
    .desc = {
        {
            .name = "order",
            .type = QEMU_OPT_STRING,
        }, {
            .name = "once",
            .type = QEMU_OPT_STRING,
        }, {
            .name = "menu",
            .type = QEMU_OPT_BOOL,
        }, {
            .name = "splash",
            .type = QEMU_OPT_STRING,
        }, {
            .name = "splash-time",
            .type = QEMU_OPT_STRING,
        }, {
            .name = "reboot-timeout",
            .type = QEMU_OPT_STRING,
        }, {
            .name = "strict",
            .type = QEMU_OPT_BOOL,
        },
        { /*End of list */ }
    },
};

static QemuOptsList qemu_add_fd_opts = {
    .name = "add-fd",
    .head = QTAILQ_HEAD_INITIALIZER(qemu_add_fd_opts.head),
    .desc = {
        {
            .name = "fd",
            .type = QEMU_OPT_NUMBER,
            .help = "file descriptor of which a duplicate is added to fd set",
        },{
            .name = "set",
            .type = QEMU_OPT_NUMBER,
            .help = "ID of the fd set to add fd to",
        },{
            .name = "opaque",
            .type = QEMU_OPT_STRING,
            .help = "free-form string used to describe fd",
        },
        { /* end of list */ }
    },
};

static QemuOptsList qemu_object_opts = {
    .name = "object",
    .implied_opt_name = "qom-type",
    .head = QTAILQ_HEAD_INITIALIZER(qemu_object_opts.head),
    .desc = {
        { }
    },
};

static QemuOptsList qemu_tpmdev_opts = {
    .name = "tpmdev",
    .implied_opt_name = "type",
    .head = QTAILQ_HEAD_INITIALIZER(qemu_tpmdev_opts.head),
    .desc = {
        /* options are defined in the TPM backends */
        { /* end of list */ }
    },
};

static QemuOptsList qemu_realtime_opts = {
    .name = "realtime",
    .head = QTAILQ_HEAD_INITIALIZER(qemu_realtime_opts.head),
    .desc = {
        {
            .name = "mlock",
            .type = QEMU_OPT_BOOL,
        },
        { /* end of list */ }
    },
};

static QemuOptsList qemu_msg_opts = {
    .name = "msg",
    .head = QTAILQ_HEAD_INITIALIZER(qemu_msg_opts.head),
    .desc = {
        {
            .name = "timestamp",
            .type = QEMU_OPT_BOOL,
        },
        { /* end of list */ }
    },
};

static QemuOptsList qemu_name_opts = {
    .name = "name",
    .implied_opt_name = "guest",
    .merge_lists = true,
    .head = QTAILQ_HEAD_INITIALIZER(qemu_name_opts.head),
    .desc = {
        {
            .name = "guest",
            .type = QEMU_OPT_STRING,
            .help = "Sets the name of the guest.\n"
                    "This name will be displayed in the SDL window caption.\n"
                    "The name will also be used for the VNC server",
        }, {
            .name = "process",
            .type = QEMU_OPT_STRING,
            .help = "Sets the name of the QEMU process, as shown in top etc",
        }, {
            .name = "debug-threads",
            .type = QEMU_OPT_BOOL,
            .help = "When enabled, name the individual threads; defaults off.\n"
                    "NOTE: The thread names are for debugging and not a\n"
                    "stable API.",
        },
        { /* End of list */ }
    },
};

static QemuOptsList qemu_mem_opts = {
    .name = "memory",
    .implied_opt_name = "size",
    .head = QTAILQ_HEAD_INITIALIZER(qemu_mem_opts.head),
    .merge_lists = true,
    .desc = {
        {
            .name = "size",
            .type = QEMU_OPT_SIZE,
        },
        {
            .name = "slots",
            .type = QEMU_OPT_NUMBER,
        },
        {
            .name = "maxmem",
            .type = QEMU_OPT_SIZE,
        },
        { /* end of list */ }
    },
};

static QemuOptsList qemu_icount_opts = {
    .name = "icount",
    .implied_opt_name = "shift",
    .merge_lists = true,
    .head = QTAILQ_HEAD_INITIALIZER(qemu_icount_opts.head),
    .desc = {
        {
            .name = "shift",
            .type = QEMU_OPT_STRING,
        }, {
            .name = "align",
            .type = QEMU_OPT_BOOL,
        },
        { /* end of list */ }
    },
};

#ifdef CONFIG_ANDROID
// Save System boot parameters from the command line
#define MAX_N_CMD_PROPS 16
static const char* cmd_props[MAX_N_CMD_PROPS];
static       int   n_cmd_props = 0;

static void save_cmd_property(const char* propStr) {
    if (n_cmd_props >= MAX_N_CMD_PROPS) {
        fprintf(stderr, "Too many command-line boot properties. "
                        "This property is ignored: \"%s\"\n", propStr);
        return;
    }
    cmd_props[n_cmd_props++] = propStr;
}

// Provide the saved System boot parameters from the command line
static void process_cmd_properties() {
    int idx;
    for(idx = 0; idx<n_cmd_props; idx++) {
        // The string should be of the form
        // "keyname=value"
        const char* pkey = cmd_props[idx];
        const char* peq = strchr(pkey, '=');
        if (peq) {
            // Pass ptr and length for both parts
            boot_property_add2(pkey, (peq - pkey),
                               peq+1, strlen(peq+1));
        }
    }
}
#endif  // CONFIG_ANDROID

/**
 * Get machine options
 *
 * Returns: machine options (never null).
 */
QemuOpts *qemu_get_machine_opts(void)
{
    return qemu_find_opts_singleton("machine");
}

const char *qemu_get_vm_name(void)
{
    return qemu_name;
}

static void res_free(void)
{
    if (boot_splash_filedata != NULL) {
        g_free(boot_splash_filedata);
        boot_splash_filedata = NULL;
    }
#ifdef CONFIG_ANDROID
    g_free(additional_kernel_params);
#endif  // CONFIG_ANDROID
}

static int default_driver_check(QemuOpts *opts, void *opaque)
{
    const char *driver = qemu_opt_get(opts, "driver");
    int i;

    if (!driver)
        return 0;
    for (i = 0; i < ARRAY_SIZE(default_list); i++) {
        if (strcmp(default_list[i].driver, driver) != 0)
            continue;
        *(default_list[i].flag) = 0;
    }
    return 0;
}

/***********************************************************/
/* QEMU state */

static RunState current_run_state = RUN_STATE_PRELAUNCH;

/* We use RUN_STATE_MAX but any invalid value will do */
static RunState vmstop_requested = RUN_STATE_MAX;
static QemuMutex vmstop_lock;

typedef struct {
    RunState from;
    RunState to;
} RunStateTransition;

static const RunStateTransition runstate_transitions_def[] = {
    /*     from      ->     to      */
    { RUN_STATE_DEBUG, RUN_STATE_RUNNING },
    { RUN_STATE_DEBUG, RUN_STATE_FINISH_MIGRATE },

    { RUN_STATE_INMIGRATE, RUN_STATE_RUNNING },
    { RUN_STATE_INMIGRATE, RUN_STATE_PAUSED },

    { RUN_STATE_INTERNAL_ERROR, RUN_STATE_PAUSED },
    { RUN_STATE_INTERNAL_ERROR, RUN_STATE_FINISH_MIGRATE },

    { RUN_STATE_IO_ERROR, RUN_STATE_RUNNING },
    { RUN_STATE_IO_ERROR, RUN_STATE_FINISH_MIGRATE },

    { RUN_STATE_PAUSED, RUN_STATE_RUNNING },
    { RUN_STATE_PAUSED, RUN_STATE_FINISH_MIGRATE },

    { RUN_STATE_POSTMIGRATE, RUN_STATE_RUNNING },
    { RUN_STATE_POSTMIGRATE, RUN_STATE_FINISH_MIGRATE },

    { RUN_STATE_PRELAUNCH, RUN_STATE_RUNNING },
    { RUN_STATE_PRELAUNCH, RUN_STATE_FINISH_MIGRATE },
    { RUN_STATE_PRELAUNCH, RUN_STATE_INMIGRATE },

    { RUN_STATE_FINISH_MIGRATE, RUN_STATE_RUNNING },
    { RUN_STATE_FINISH_MIGRATE, RUN_STATE_POSTMIGRATE },

    { RUN_STATE_RESTORE_VM, RUN_STATE_RUNNING },

    { RUN_STATE_RUNNING, RUN_STATE_DEBUG },
    { RUN_STATE_RUNNING, RUN_STATE_INTERNAL_ERROR },
    { RUN_STATE_RUNNING, RUN_STATE_IO_ERROR },
    { RUN_STATE_RUNNING, RUN_STATE_PAUSED },
    { RUN_STATE_RUNNING, RUN_STATE_FINISH_MIGRATE },
    { RUN_STATE_RUNNING, RUN_STATE_RESTORE_VM },
    { RUN_STATE_RUNNING, RUN_STATE_SAVE_VM },
    { RUN_STATE_RUNNING, RUN_STATE_SHUTDOWN },
    { RUN_STATE_RUNNING, RUN_STATE_WATCHDOG },
    { RUN_STATE_RUNNING, RUN_STATE_GUEST_PANICKED },

    { RUN_STATE_SAVE_VM, RUN_STATE_RUNNING },

    { RUN_STATE_SHUTDOWN, RUN_STATE_PAUSED },
    { RUN_STATE_SHUTDOWN, RUN_STATE_FINISH_MIGRATE },

    { RUN_STATE_DEBUG, RUN_STATE_SUSPENDED },
    { RUN_STATE_RUNNING, RUN_STATE_SUSPENDED },
    { RUN_STATE_SUSPENDED, RUN_STATE_RUNNING },
    { RUN_STATE_SUSPENDED, RUN_STATE_FINISH_MIGRATE },

    { RUN_STATE_WATCHDOG, RUN_STATE_RUNNING },
    { RUN_STATE_WATCHDOG, RUN_STATE_FINISH_MIGRATE },

    { RUN_STATE_GUEST_PANICKED, RUN_STATE_RUNNING },
    { RUN_STATE_GUEST_PANICKED, RUN_STATE_FINISH_MIGRATE },

    { RUN_STATE_MAX, RUN_STATE_MAX },
};

static bool runstate_valid_transitions[RUN_STATE_MAX][RUN_STATE_MAX];

bool runstate_check(RunState state)
{
    return current_run_state == state;
}

static void runstate_init(void)
{
    const RunStateTransition *p;

    memset(&runstate_valid_transitions, 0, sizeof(runstate_valid_transitions));
    for (p = &runstate_transitions_def[0]; p->from != RUN_STATE_MAX; p++) {
        runstate_valid_transitions[p->from][p->to] = true;
    }

    qemu_mutex_init(&vmstop_lock);
}

/* This function will abort() on invalid state transitions */
void runstate_set(RunState new_state)
{
    assert(new_state < RUN_STATE_MAX);

    if (!runstate_valid_transitions[current_run_state][new_state]) {
        fprintf(stderr, "ERROR: invalid runstate transition: '%s' -> '%s'\n",
                RunState_lookup[current_run_state],
                RunState_lookup[new_state]);
        abort();
    }
    trace_runstate_set(new_state);
    current_run_state = new_state;
}

int runstate_is_running(void)
{
    return runstate_check(RUN_STATE_RUNNING);
}

bool runstate_needs_reset(void)
{
    return runstate_check(RUN_STATE_INTERNAL_ERROR) ||
        runstate_check(RUN_STATE_SHUTDOWN);
}

StatusInfo *qmp_query_status(Error **errp)
{
    StatusInfo *info = g_malloc0(sizeof(*info));

    info->running = runstate_is_running();
    info->singlestep = singlestep;
    info->status = current_run_state;

    return info;
}

static bool qemu_vmstop_requested(RunState *r)
{
    qemu_mutex_lock(&vmstop_lock);
    *r = vmstop_requested;
    vmstop_requested = RUN_STATE_MAX;
    qemu_mutex_unlock(&vmstop_lock);
    return *r < RUN_STATE_MAX;
}

void qemu_system_vmstop_request_prepare(void)
{
    qemu_mutex_lock(&vmstop_lock);
}

void qemu_system_vmstop_request(RunState state)
{
    vmstop_requested = state;
    qemu_mutex_unlock(&vmstop_lock);
    qemu_notify_event();
}

void vm_start(void)
{
    RunState requested;

    qemu_vmstop_requested(&requested);
    if (runstate_is_running() && requested == RUN_STATE_MAX) {
        return;
    }

    /* Ensure that a STOP/RESUME pair of events is emitted if a
     * vmstop request was pending.  The BLOCK_IO_ERROR event, for
     * example, according to documentation is always followed by
     * the STOP event.
     */
    if (runstate_is_running()) {
        qapi_event_send_stop(&error_abort);
    } else {
        cpu_enable_ticks();
        runstate_set(RUN_STATE_RUNNING);
        vm_state_notify(1, RUN_STATE_RUNNING);
        resume_all_vcpus();
    }

    qapi_event_send_resume(&error_abort);
}


/***********************************************************/
/* real time host monotonic timer */

/***********************************************************/
/* host time/date access */
void qemu_get_timedate(struct tm *tm, int offset)
{
    time_t ti;

    time(&ti);
    ti += offset;
    if (rtc_date_offset == -1) {
        if (rtc_utc)
            gmtime_r(&ti, tm);
        else
            localtime_r(&ti, tm);
    } else {
        ti -= rtc_date_offset;
        gmtime_r(&ti, tm);
    }
}

int qemu_timedate_diff(struct tm *tm)
{
    time_t seconds;

    if (rtc_date_offset == -1)
        if (rtc_utc)
            seconds = mktimegm(tm);
        else {
            struct tm tmp = *tm;
            tmp.tm_isdst = -1; /* use timezone to figure it out */
            seconds = mktime(&tmp);
	}
    else
        seconds = mktimegm(tm) + rtc_date_offset;

    return seconds - time(NULL);
}

static bool configure_rtc_date_offset(const char *startdate, int legacy)
{
    time_t rtc_start_date;
    struct tm tm;

    if (!strcmp(startdate, "now") && legacy) {
        rtc_date_offset = -1;
    } else {
        if (sscanf(startdate, "%d-%d-%dT%d:%d:%d",
                   &tm.tm_year,
                   &tm.tm_mon,
                   &tm.tm_mday,
                   &tm.tm_hour,
                   &tm.tm_min,
                   &tm.tm_sec) == 6) {
            /* OK */
        } else if (sscanf(startdate, "%d-%d-%d",
                          &tm.tm_year,
                          &tm.tm_mon,
                          &tm.tm_mday) == 3) {
            tm.tm_hour = 0;
            tm.tm_min = 0;
            tm.tm_sec = 0;
        } else {
            goto date_fail;
        }
        tm.tm_year -= 1900;
        tm.tm_mon--;
        rtc_start_date = mktimegm(&tm);
        if (rtc_start_date == -1) {
        date_fail:
            fprintf(stderr, "Invalid date format. Valid formats are:\n"
                            "'2006-06-17T16:01:21' or '2006-06-17'\n");
            return false;
        }
        rtc_date_offset = time(NULL) - rtc_start_date;
    }

    return true;
}

static bool configure_rtc(QemuOpts *opts)
{
    const char *value;

    value = qemu_opt_get(opts, "base");
    if (value) {
        if (!strcmp(value, "utc")) {
            rtc_utc = 1;
        } else if (!strcmp(value, "localtime")) {
            rtc_utc = 0;
        } else {
            if (!configure_rtc_date_offset(value, 0)) {
                return false;
            }
        }
    }
    value = qemu_opt_get(opts, "clock");
    if (value) {
        if (!strcmp(value, "host")) {
            rtc_clock = QEMU_CLOCK_HOST;
        } else if (!strcmp(value, "rt")) {
            rtc_clock = QEMU_CLOCK_REALTIME;
        } else if (!strcmp(value, "vm")) {
            rtc_clock = QEMU_CLOCK_VIRTUAL;
        } else {
            fprintf(stderr, "qemu: invalid option value '%s'\n", value);
            return false;
        }
    }
    value = qemu_opt_get(opts, "driftfix");
    if (value) {
        if (!strcmp(value, "slew")) {
            static GlobalProperty slew_lost_ticks[] = {
                {
                    .driver   = "mc146818rtc",
                    .property = "lost_tick_policy",
                    .value    = "slew",
                },
                { /* end of list */ }
            };

            qdev_prop_register_global_list(slew_lost_ticks);
        } else if (!strcmp(value, "none")) {
            /* discard is default */
        } else {
            fprintf(stderr, "qemu: invalid option value '%s'\n", value);
            return false;
        }
    }

    return true;
}

/***********************************************************/
/* Bluetooth support */
static int nb_hcis;
static int cur_hci;
static struct HCIInfo *hci_table[MAX_NICS];

struct HCIInfo *qemu_next_hci(void)
{
    if (cur_hci == nb_hcis)
        return &null_hci;

    return hci_table[cur_hci++];
}

static int bt_hci_parse(const char *str)
{
    struct HCIInfo *hci;
    bdaddr_t bdaddr;

    if (nb_hcis >= MAX_NICS) {
        fprintf(stderr, "qemu: Too many bluetooth HCIs (max %i).\n", MAX_NICS);
        return -1;
    }

    hci = hci_init(str);
    if (!hci)
        return -1;

    bdaddr.b[0] = 0x52;
    bdaddr.b[1] = 0x54;
    bdaddr.b[2] = 0x00;
    bdaddr.b[3] = 0x12;
    bdaddr.b[4] = 0x34;
    bdaddr.b[5] = 0x56 + nb_hcis;
    hci->bdaddr_set(hci, bdaddr.b);

    hci_table[nb_hcis++] = hci;

    return 0;
}

static void bt_vhci_add(int vlan_id)
{
    struct bt_scatternet_s *vlan = qemu_find_bt_vlan(vlan_id);

    if (!vlan->slave)
        fprintf(stderr, "qemu: warning: adding a VHCI to "
                        "an empty scatternet %i\n", vlan_id);

    bt_vhci_init(bt_new_hci(vlan));
}

static struct bt_device_s *bt_device_add(const char *opt)
{
    struct bt_scatternet_s *vlan;
    int vlan_id = 0;
    char *endp = strstr(opt, ",vlan=");
    int len = (endp ? endp - opt : strlen(opt)) + 1;
    char devname[10];

    pstrcpy(devname, MIN(sizeof(devname), len), opt);

    if (endp) {
        vlan_id = strtol(endp + 6, &endp, 0);
        if (*endp) {
            fprintf(stderr, "qemu: unrecognised bluetooth vlan Id\n");
            return 0;
        }
    }

    vlan = qemu_find_bt_vlan(vlan_id);

    if (!vlan->slave)
        fprintf(stderr, "qemu: warning: adding a slave device to "
                        "an empty scatternet %i\n", vlan_id);

    if (!strcmp(devname, "keyboard"))
        return bt_keyboard_init(vlan);

    fprintf(stderr, "qemu: unsupported bluetooth device `%s'\n", devname);
    return 0;
}

static int bt_parse(const char *opt)
{
    const char *endp, *p;
    int vlan;

    if (strstart(opt, "hci", &endp)) {
        if (!*endp || *endp == ',') {
            if (*endp)
                if (!strstart(endp, ",vlan=", 0))
                    opt = endp + 1;

            return bt_hci_parse(opt);
       }
    } else if (strstart(opt, "vhci", &endp)) {
        if (!*endp || *endp == ',') {
            if (*endp) {
                if (strstart(endp, ",vlan=", &p)) {
                    vlan = strtol(p, (char **) &endp, 0);
                    if (*endp) {
                        fprintf(stderr, "qemu: bad scatternet '%s'\n", p);
                        return 1;
                    }
                } else {
                    fprintf(stderr, "qemu: bad parameter '%s'\n", endp + 1);
                    return 1;
                }
            } else
                vlan = 0;

            bt_vhci_add(vlan);
            return 0;
        }
    } else if (strstart(opt, "device:", &endp))
        return !bt_device_add(endp);

    fprintf(stderr, "qemu: bad bluetooth parameter '%s'\n", opt);
    return 1;
}

static int parse_sandbox(QemuOpts *opts, void *opaque)
{
    /* FIXME: change this to true for 1.3 */
    if (qemu_opt_get_bool(opts, "enable", false)) {
#ifdef CONFIG_SECCOMP
        if (seccomp_start() < 0) {
            qerror_report(ERROR_CLASS_GENERIC_ERROR,
                          "failed to install seccomp syscall filter in the kernel");
            return -1;
        }
#else
        qerror_report(ERROR_CLASS_GENERIC_ERROR,
                      "sandboxing request but seccomp is not compiled into this build");
        return -1;
#endif
    }

    return 0;
}

static int parse_name(QemuOpts *opts, void *opaque)
{
    const char *proc_name;

    if (qemu_opt_get(opts, "debug-threads")) {
        qemu_thread_naming(qemu_opt_get_bool(opts, "debug-threads", false));
    }
    qemu_name = qemu_opt_get(opts, "guest");

    proc_name = qemu_opt_get(opts, "process");
    if (proc_name) {
        os_set_proc_name(proc_name);
    }

    return 0;
}

bool usb_enabled(bool default_usb)
{
    return qemu_opt_get_bool(qemu_get_machine_opts(), "usb",
                             has_defaults && default_usb);
}

#ifndef _WIN32
static int parse_add_fd(QemuOpts *opts, void *opaque)
{
    int fd, dupfd, flags;
    int64_t fdset_id;
    const char *fd_opaque = NULL;

    fd = qemu_opt_get_number(opts, "fd", -1);
    fdset_id = qemu_opt_get_number(opts, "set", -1);
    fd_opaque = qemu_opt_get(opts, "opaque");

    if (fd < 0) {
        qerror_report(ERROR_CLASS_GENERIC_ERROR,
                      "fd option is required and must be non-negative");
        return -1;
    }

    if (fd <= STDERR_FILENO) {
        qerror_report(ERROR_CLASS_GENERIC_ERROR,
                      "fd cannot be a standard I/O stream");
        return -1;
    }

    /*
     * All fds inherited across exec() necessarily have FD_CLOEXEC
     * clear, while qemu sets FD_CLOEXEC on all other fds used internally.
     */
    flags = fcntl(fd, F_GETFD);
    if (flags == -1 || (flags & FD_CLOEXEC)) {
        qerror_report(ERROR_CLASS_GENERIC_ERROR,
                      "fd is not valid or already in use");
        return -1;
    }

    if (fdset_id < 0) {
        qerror_report(ERROR_CLASS_GENERIC_ERROR,
                      "set option is required and must be non-negative");
        return -1;
    }

#ifdef F_DUPFD_CLOEXEC
    dupfd = fcntl(fd, F_DUPFD_CLOEXEC, 0);
#else
    dupfd = dup(fd);
    if (dupfd != -1) {
        qemu_set_cloexec(dupfd);
    }
#endif
    if (dupfd == -1) {
        qerror_report(ERROR_CLASS_GENERIC_ERROR,
                      "Error duplicating fd: %s", strerror(errno));
        return -1;
    }

    /* add the duplicate fd, and optionally the opaque string, to the fd set */
    monitor_fdset_add_fd(dupfd, true, fdset_id, fd_opaque ? true : false,
                         fd_opaque, NULL);

    return 0;
}

static int cleanup_add_fd(QemuOpts *opts, void *opaque)
{
    int fd;

    fd = qemu_opt_get_number(opts, "fd", -1);
    close(fd);

    return 0;
}
#endif

/***********************************************************/
/* QEMU Block devices */

#define HD_OPTS "media=disk"
#define CDROM_OPTS "media=cdrom"
#define FD_OPTS ""
#define PFLASH_OPTS ""
#define MTD_OPTS ""
#define SD_OPTS ""

static int drive_init_func(QemuOpts *opts, void *opaque)
{
    BlockInterfaceType *block_default_type = opaque;

    return drive_new(opts, *block_default_type) == NULL;
}

static int drive_enable_snapshot(QemuOpts *opts, void *opaque)
{
    if (qemu_opt_get(opts, "snapshot") == NULL) {
        qemu_opt_set(opts, "snapshot", "on");
    }
    return 0;
}

static bool default_drive(int enable, int snapshot, BlockInterfaceType type,
                          int index, const char *optstr)
{
    QemuOpts *opts;
    DriveInfo *dinfo;

    if (!enable || drive_get_by_index(type, index)) {
        return true;
    }

    opts = drive_add(type, index, NULL, optstr);
    if (snapshot) {
        drive_enable_snapshot(opts, NULL);
    }

    dinfo = drive_new(opts, type);
    if (!dinfo) {
        return false;
    }
    dinfo->is_default = true;
    return true;
}

void qemu_register_boot_set(QEMUBootSetHandler *func, void *opaque)
{
    boot_set_handler = func;
    boot_set_opaque = opaque;
}

int qemu_boot_set(const char *boot_order)
{
    if (!boot_set_handler) {
        return -EINVAL;
    }
    return boot_set_handler(boot_set_opaque, boot_order);
}

static bool validate_bootdevices(const char *devices)
{
    /* We just do some generic consistency checks */
    const char *p;
    int bitmap = 0;

    for (p = devices; *p != '\0'; p++) {
        /* Allowed boot devices are:
         * a-b: floppy disk drives
         * c-f: IDE disk drives
         * g-m: machine implementation dependent drives
         * n-p: network devices
         * It's up to each machine implementation to check if the given boot
         * devices match the actual hardware implementation and firmware
         * features.
         */
        if (*p < 'a' || *p > 'p') {
            fprintf(stderr, "Invalid boot device '%c'\n", *p);
            return false;
        }
        if (bitmap & (1 << (*p - 'a'))) {
            fprintf(stderr, "Boot device '%c' was given twice\n", *p);
            return false;
        }
        bitmap |= 1 << (*p - 'a');
    }

    return true;
}

static void restore_boot_order(void *opaque)
{
    char *normal_boot_order = opaque;
    static int first = 1;

    /* Restore boot order and remove ourselves after the first boot */
    if (first) {
        first = 0;
        return;
    }

    qemu_boot_set(normal_boot_order);

    qemu_unregister_reset(restore_boot_order, normal_boot_order);
    g_free(normal_boot_order);
}

static QemuOptsList qemu_smp_opts = {
    .name = "smp-opts",
    .implied_opt_name = "cpus",
    .merge_lists = true,
    .head = QTAILQ_HEAD_INITIALIZER(qemu_smp_opts.head),
    .desc = {
        {
            .name = "cpus",
            .type = QEMU_OPT_NUMBER,
        }, {
            .name = "sockets",
            .type = QEMU_OPT_NUMBER,
        }, {
            .name = "cores",
            .type = QEMU_OPT_NUMBER,
        }, {
            .name = "threads",
            .type = QEMU_OPT_NUMBER,
        }, {
            .name = "maxcpus",
            .type = QEMU_OPT_NUMBER,
        },
        { /*End of list */ }
    },
};

static bool smp_parse(QemuOpts *opts)
{
    if (opts) {

        unsigned cpus    = qemu_opt_get_number(opts, "cpus", 0);
        unsigned sockets = qemu_opt_get_number(opts, "sockets", 0);
        unsigned cores   = qemu_opt_get_number(opts, "cores", 0);
        unsigned threads = qemu_opt_get_number(opts, "threads", 0);

        /* compute missing values, prefer sockets over cores over threads */
        if (cpus == 0 || sockets == 0) {
            sockets = sockets > 0 ? sockets : 1;
            cores = cores > 0 ? cores : 1;
            threads = threads > 0 ? threads : 1;
            if (cpus == 0) {
                cpus = cores * threads * sockets;
            }
        } else {
            if (cores == 0) {
                threads = threads > 0 ? threads : 1;
                cores = cpus / (sockets * threads);
            } else {
                threads = cpus / (cores * sockets);
            }
        }

        max_cpus = qemu_opt_get_number(opts, "maxcpus", 0);

        smp_cpus = cpus;
        smp_cores = cores > 0 ? cores : 1;
        smp_threads = threads > 0 ? threads : 1;

    }

    if (max_cpus == 0) {
        max_cpus = smp_cpus;
    }

    if (max_cpus > MAX_CPUMASK_BITS) {
        fprintf(stderr, "Unsupported number of maxcpus\n");
        return false;
    }
    if (max_cpus < smp_cpus) {
        fprintf(stderr, "maxcpus must be equal to or greater than smp\n");
        return false;
    }

    return true;
}

static bool realtime_init(void)
{
    if (enable_mlock) {
        if (os_mlock() < 0) {
            fprintf(stderr, "qemu: locking memory failed\n");
            return false;
        }
    }

    return true;
}


static void configure_msg(QemuOpts *opts)
{
    enable_timestamp_msg = qemu_opt_get_bool(opts, "timestamp", true);
}

/***********************************************************/
/* USB devices */

static int usb_device_add(const char *devname)
{
    USBDevice *dev = NULL;
#ifndef CONFIG_LINUX
    const char *p;
#endif

    if (!usb_enabled(false)) {
        return -1;
    }

    /* drivers with .usbdevice_name entry in USBDeviceInfo */
    dev = usbdevice_create(devname);
    if (dev)
        goto done;

    /* the other ones */
#ifndef CONFIG_LINUX
    /* only the linux version is qdev-ified, usb-bsd still needs this */
    if (strstart(devname, "host:", &p)) {
        dev = usb_host_device_open(usb_bus_find(-1), p);
    }
#endif
    if (!dev)
        return -1;

done:
    return 0;
}

static int usb_device_del(const char *devname)
{
    int bus_num, addr;
    const char *p;

    if (strstart(devname, "host:", &p)) {
        return -1;
    }

    if (!usb_enabled(false)) {
        return -1;
    }

    p = strchr(devname, '.');
    if (!p)
        return -1;
    bus_num = strtoul(devname, NULL, 0);
    addr = strtoul(p + 1, NULL, 0);

    return usb_device_delete_addr(bus_num, addr);
}

static int usb_parse(const char *cmdline)
{
    int r;
    r = usb_device_add(cmdline);
    if (r < 0) {
        fprintf(stderr, "qemu: could not add USB device '%s'\n", cmdline);
    }
    return r;
}

void do_usb_add(Monitor *mon, const QDict *qdict)
{
    const char *devname = qdict_get_str(qdict, "devname");
    if (usb_device_add(devname) < 0) {
        error_report("could not add USB device '%s'", devname);
    }
}

void do_usb_del(Monitor *mon, const QDict *qdict)
{
    const char *devname = qdict_get_str(qdict, "devname");
    if (usb_device_del(devname) < 0) {
        error_report("could not delete USB device '%s'", devname);
    }
}

/***********************************************************/
/* machine registration */

MachineState *current_machine;

static void machine_class_init(ObjectClass *oc, void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);
    QEMUMachine *qm = data;

    mc->family = qm->family;
    mc->name = qm->name;
    mc->alias = qm->alias;
    mc->desc = qm->desc;
    mc->init = qm->init;
    mc->reset = qm->reset;
    mc->hot_add_cpu = qm->hot_add_cpu;
    mc->kvm_type = qm->kvm_type;
    mc->block_default_type = qm->block_default_type;
    mc->units_per_default_bus = qm->units_per_default_bus;
    mc->max_cpus = qm->max_cpus;
    mc->no_serial = qm->no_serial;
    mc->no_parallel = qm->no_parallel;
    mc->use_virtcon = qm->use_virtcon;
    mc->use_sclp = qm->use_sclp;
    mc->no_floppy = qm->no_floppy;
    mc->no_cdrom = qm->no_cdrom;
    mc->no_sdcard = qm->no_sdcard;
    mc->has_dynamic_sysbus = qm->has_dynamic_sysbus;
    mc->is_default = qm->is_default;
    mc->default_machine_opts = qm->default_machine_opts;
    mc->default_boot_order = qm->default_boot_order;
    mc->default_display = qm->default_display;
    mc->compat_props = qm->compat_props;
    mc->hw_version = qm->hw_version;
}

int qemu_register_machine(QEMUMachine *m)
{
    char *name = g_strconcat(m->name, TYPE_MACHINE_SUFFIX, NULL);
    TypeInfo ti = {
        .name       = name,
        .parent     = TYPE_MACHINE,
        .class_init = machine_class_init,
        .class_data = (void *)m,
    };

    type_register(&ti);
    g_free(name);

    return 0;
}

static MachineClass *find_machine(const char *name)
{
    GSList *el, *machines = object_class_get_list(TYPE_MACHINE, false);
    MachineClass *mc = NULL;

    for (el = machines; el; el = el->next) {
        MachineClass *temp = el->data;

        if (!strcmp(temp->name, name)) {
            mc = temp;
            break;
        }
        if (temp->alias &&
            !strcmp(temp->alias, name)) {
            mc = temp;
            break;
        }
    }

    g_slist_free(machines);
    return mc;
}

MachineClass *find_default_machine(void)
{
    GSList *el, *machines = object_class_get_list(TYPE_MACHINE, false);
    MachineClass *mc = NULL;

    for (el = machines; el; el = el->next) {
        MachineClass *temp = el->data;

        if (temp->is_default) {
            mc = temp;
            break;
        }
    }

    g_slist_free(machines);
    return mc;
}

MachineInfoList *qmp_query_machines(Error **errp)
{
    GSList *el, *machines = object_class_get_list(TYPE_MACHINE, false);
    MachineInfoList *mach_list = NULL;

    for (el = machines; el; el = el->next) {
        MachineClass *mc = el->data;
        MachineInfoList *entry;
        MachineInfo *info;

        info = g_malloc0(sizeof(*info));
        if (mc->is_default) {
            info->has_is_default = true;
            info->is_default = true;
        }

        if (mc->alias) {
            info->has_alias = true;
            info->alias = g_strdup(mc->alias);
        }

        info->name = g_strdup(mc->name);
        info->cpu_max = !mc->max_cpus ? 1 : mc->max_cpus;

        entry = g_malloc0(sizeof(*entry));
        entry->value = info;
        entry->next = mach_list;
        mach_list = entry;
    }

    g_slist_free(machines);
    return mach_list;
}

/***********************************************************/
/* main execution loop */

struct vm_change_state_entry {
    VMChangeStateHandler *cb;
    void *opaque;
    QLIST_ENTRY (vm_change_state_entry) entries;
};

static QLIST_HEAD(vm_change_state_head, vm_change_state_entry) vm_change_state_head;

VMChangeStateEntry *qemu_add_vm_change_state_handler(VMChangeStateHandler *cb,
                                                     void *opaque)
{
    VMChangeStateEntry *e;

    e = g_malloc0(sizeof (*e));

    e->cb = cb;
    e->opaque = opaque;
    QLIST_INSERT_HEAD(&vm_change_state_head, e, entries);
    return e;
}

void qemu_del_vm_change_state_handler(VMChangeStateEntry *e)
{
    QLIST_REMOVE (e, entries);
    g_free (e);
}

void vm_state_notify(int running, RunState state)
{
    VMChangeStateEntry *e, *next;

    trace_vm_state_notify(running, state);

    QLIST_FOREACH_SAFE(e, &vm_change_state_head, entries, next) {
        e->cb(e->opaque, running, state);
    }
}

/* reset/shutdown handler */

typedef struct QEMUResetEntry {
    QTAILQ_ENTRY(QEMUResetEntry) entry;
    QEMUResetHandler *func;
    void *opaque;
} QEMUResetEntry;

static QTAILQ_HEAD(reset_handlers, QEMUResetEntry) reset_handlers =
    QTAILQ_HEAD_INITIALIZER(reset_handlers);
static int reset_requested;
static int shutdown_requested, shutdown_signal = -1;
static pid_t shutdown_pid;
static int powerdown_requested;
static int debug_requested;
static int suspend_requested;
static WakeupReason wakeup_reason;
static NotifierList powerdown_notifiers =
    NOTIFIER_LIST_INITIALIZER(powerdown_notifiers);
static NotifierList suspend_notifiers =
    NOTIFIER_LIST_INITIALIZER(suspend_notifiers);
static NotifierList wakeup_notifiers =
    NOTIFIER_LIST_INITIALIZER(wakeup_notifiers);
static uint32_t wakeup_reason_mask = ~(1 << QEMU_WAKEUP_REASON_NONE);

int qemu_shutdown_requested_get(void)
{
    return shutdown_requested;
}

int qemu_reset_requested_get(void)
{
    return reset_requested;
}

static int qemu_shutdown_requested(void)
{
    return atomic_xchg(&shutdown_requested, 0);
}

static void qemu_kill_report(void)
{
    if (!qtest_driver() && shutdown_signal != -1) {
        fprintf(stderr, "qemu: terminating on signal %d", shutdown_signal);
        if (shutdown_pid == 0) {
            /* This happens for eg ^C at the terminal, so it's worth
             * avoiding printing an odd message in that case.
             */
            fputc('\n', stderr);
        } else {
            fprintf(stderr, " from pid " FMT_pid "\n", shutdown_pid);
        }
        shutdown_signal = -1;
    }
}

static int qemu_reset_requested(void)
{
    int r = reset_requested;
    reset_requested = 0;
    return r;
}

static int qemu_suspend_requested(void)
{
    int r = suspend_requested;
    suspend_requested = 0;
    return r;
}

static WakeupReason qemu_wakeup_requested(void)
{
    return wakeup_reason;
}

static int qemu_powerdown_requested(void)
{
    int r = powerdown_requested;
    powerdown_requested = 0;
    return r;
}

static int qemu_debug_requested(void)
{
    int r = debug_requested;
    debug_requested = 0;
    return r;
}

void qemu_register_reset(QEMUResetHandler *func, void *opaque)
{
    QEMUResetEntry *re = g_malloc0(sizeof(QEMUResetEntry));

    re->func = func;
    re->opaque = opaque;
    QTAILQ_INSERT_TAIL(&reset_handlers, re, entry);
}

void qemu_unregister_reset(QEMUResetHandler *func, void *opaque)
{
    QEMUResetEntry *re;

    QTAILQ_FOREACH(re, &reset_handlers, entry) {
        if (re->func == func && re->opaque == opaque) {
            QTAILQ_REMOVE(&reset_handlers, re, entry);
            g_free(re);
            return;
        }
    }
}

void qemu_devices_reset(void)
{
    QEMUResetEntry *re, *nre;

    /* reset all devices */
    QTAILQ_FOREACH_SAFE(re, &reset_handlers, entry, nre) {
        re->func(re->opaque);
    }
}

void qemu_system_reset(bool report)
{
    MachineClass *mc;

    mc = current_machine ? MACHINE_GET_CLASS(current_machine) : NULL;

    if (mc && mc->reset) {
        mc->reset();
    } else {
        qemu_devices_reset();
    }
    if (report) {
        qapi_event_send_reset(&error_abort);
    }
    cpu_synchronize_all_post_reset();
}

void qemu_system_reset_request(void)
{
    if (no_reboot) {
        shutdown_requested = 1;
    } else {
        reset_requested = 1;
    }
    cpu_stop_current();
    qemu_notify_event();
}

static void qemu_system_suspend(void)
{
    pause_all_vcpus();
    notifier_list_notify(&suspend_notifiers, NULL);
    runstate_set(RUN_STATE_SUSPENDED);
    qapi_event_send_suspend(&error_abort);
}

void qemu_system_suspend_request(void)
{
    if (runstate_check(RUN_STATE_SUSPENDED)) {
        return;
    }
    suspend_requested = 1;
    cpu_stop_current();
    qemu_notify_event();
}

void qemu_register_suspend_notifier(Notifier *notifier)
{
    notifier_list_add(&suspend_notifiers, notifier);
}

void qemu_system_wakeup_request(WakeupReason reason)
{
    trace_system_wakeup_request(reason);

    if (!runstate_check(RUN_STATE_SUSPENDED)) {
        return;
    }
    if (!(wakeup_reason_mask & (1 << reason))) {
        return;
    }
    runstate_set(RUN_STATE_RUNNING);
    wakeup_reason = reason;
    qemu_notify_event();
}

void qemu_system_wakeup_enable(WakeupReason reason, bool enabled)
{
    if (enabled) {
        wakeup_reason_mask |= (1 << reason);
    } else {
        wakeup_reason_mask &= ~(1 << reason);
    }
}

void qemu_register_wakeup_notifier(Notifier *notifier)
{
    notifier_list_add(&wakeup_notifiers, notifier);
}

void qemu_system_killed(int signal, pid_t pid)
{
    shutdown_signal = signal;
    shutdown_pid = pid;
    no_shutdown = 0;
    qemu_system_shutdown_request();
}

void qemu_system_shutdown_request(void)
{
    trace_qemu_system_shutdown_request();
    shutdown_requested = 1;
    qemu_notify_event();
}

static void qemu_system_powerdown(void)
{
    qapi_event_send_powerdown(&error_abort);
    notifier_list_notify(&powerdown_notifiers, NULL);
}

void qemu_system_powerdown_request(void)
{
    trace_qemu_system_powerdown_request();
    powerdown_requested = 1;
    qemu_notify_event();
}

void qemu_register_powerdown_notifier(Notifier *notifier)
{
    notifier_list_add(&powerdown_notifiers, notifier);
}

void qemu_system_debug_request(void)
{
    debug_requested = 1;
    qemu_notify_event();
}

static bool main_loop_should_exit(void)
{
    RunState r;
    if (qemu_debug_requested()) {
        vm_stop(RUN_STATE_DEBUG);
    }
    if (qemu_suspend_requested()) {
        qemu_system_suspend();
    }
    if (qemu_shutdown_requested()) {
        qemu_kill_report();
        qapi_event_send_shutdown(&error_abort);
        if (no_shutdown) {
            vm_stop(RUN_STATE_SHUTDOWN);
        } else {
            return true;
        }
    }
    if (qemu_reset_requested()) {
        pause_all_vcpus();
        cpu_synchronize_all_states();
        qemu_system_reset(VMRESET_REPORT);
        resume_all_vcpus();
        if (runstate_needs_reset()) {
            runstate_set(RUN_STATE_PAUSED);
        }
    }
    if (qemu_wakeup_requested()) {
        pause_all_vcpus();
        cpu_synchronize_all_states();
        qemu_system_reset(VMRESET_SILENT);
        notifier_list_notify(&wakeup_notifiers, &wakeup_reason);
        wakeup_reason = QEMU_WAKEUP_REASON_NONE;
        resume_all_vcpus();
        qapi_event_send_wakeup(&error_abort);
    }
    if (qemu_powerdown_requested()) {
        qemu_system_powerdown();
    }
    if (qemu_vmstop_requested(&r)) {
        vm_stop(r);
    }
    return false;
}

static void main_loop(void)
{
    bool nonblocking;
    int last_io = 0;
#ifdef CONFIG_PROFILER
    int64_t ti;
#endif

    if (hax_sync_vcpus() < 0) {
        fprintf(stderr, "Internal error: hax sync failed\n");
        return;
    }

    do {
        nonblocking = !kvm_enabled() && !xen_enabled() && !hax_enabled() && last_io > 0;
#ifdef CONFIG_PROFILER
        ti = profile_getclock();
#endif
        last_io = main_loop_wait(nonblocking);
#ifdef CONFIG_PROFILER
        dev_time += profile_getclock() - ti;
#endif
    } while (!main_loop_should_exit());
}

static void version(void)
{
    printf("QEMU emulator version " QEMU_VERSION " " QEMU_PKGVERSION ", Copyright (c) 2003-2008 Fabrice Bellard\n");
}

static void help()
{
    version();
    printf("usage: %s [options] [disk_image]\n\n"
           "'disk_image' is a raw hard disk image for IDE hard disk 0\n\n",
            error_get_progname());

#define QEMU_OPTIONS_GENERATE_HELP
#include "qemu-options-wrapper.h"

    printf("\nDuring emulation, the following keys are useful:\n"
           "ctrl-alt-f      toggle full screen\n"
           "ctrl-alt-n      switch to virtual console 'n'\n"
           "ctrl-alt        toggle mouse and keyboard grab\n"
           "\n"
           "When using -nographic, press 'ctrl-a h' to get some help.\n");
}

#define HAS_ARG 0x0001

typedef struct QEMUOption {
    const char *name;
    int flags;
    int index;
    uint32_t arch_mask;
} QEMUOption;

static const QEMUOption qemu_options[] = {
    { "h", 0, QEMU_OPTION_h, QEMU_ARCH_ALL },
#define QEMU_OPTIONS_GENERATE_OPTIONS
#include "qemu-options-wrapper.h"
    { NULL },
};

static bool vga_available(void)
{
    return object_class_by_name("VGA") || object_class_by_name("isa-vga");
}

static bool cirrus_vga_available(void)
{
    return object_class_by_name("cirrus-vga")
           || object_class_by_name("isa-cirrus-vga");
}

static bool vmware_vga_available(void)
{
    return object_class_by_name("vmware-svga");
}

static bool qxl_vga_available(void)
{
    return object_class_by_name("qxl-vga");
}

static bool tcx_vga_available(void)
{
    return object_class_by_name("SUNW,tcx");
}

static bool cg3_vga_available(void)
{
    return object_class_by_name("cgthree");
}

static bool select_vgahw (const char *p)
{
    const char *opts;

    assert(vga_interface_type == VGA_NONE);
    if (strstart(p, "std", &opts)) {
        if (vga_available()) {
            vga_interface_type = VGA_STD;
        } else {
            fprintf(stderr, "Error: standard VGA not available\n");
            return false;
        }
    } else if (strstart(p, "cirrus", &opts)) {
        if (cirrus_vga_available()) {
            vga_interface_type = VGA_CIRRUS;
        } else {
            fprintf(stderr, "Error: Cirrus VGA not available\n");
            return false;
        }
    } else if (strstart(p, "vmware", &opts)) {
        if (vmware_vga_available()) {
            vga_interface_type = VGA_VMWARE;
        } else {
            fprintf(stderr, "Error: VMWare SVGA not available\n");
            return false;
        }
    } else if (strstart(p, "xenfb", &opts)) {
        vga_interface_type = VGA_XENFB;
    } else if (strstart(p, "qxl", &opts)) {
        if (qxl_vga_available()) {
            vga_interface_type = VGA_QXL;
        } else {
            fprintf(stderr, "Error: QXL VGA not available\n");
            return false;
        }
    } else if (strstart(p, "tcx", &opts)) {
        if (tcx_vga_available()) {
            vga_interface_type = VGA_TCX;
        } else {
            fprintf(stderr, "Error: TCX framebuffer not available\n");
            return false;
        }
    } else if (strstart(p, "cg3", &opts)) {
        if (cg3_vga_available()) {
            vga_interface_type = VGA_CG3;
        } else {
            fprintf(stderr, "Error: CG3 framebuffer not available\n");
            return false;
        }
    } else if (!strstart(p, "none", &opts)) {
    invalid_vga:
        fprintf(stderr, "Unknown vga type: %s\n", p);
        return false;
    }
    while (*opts) {
        const char *nextopt;

        if (strstart(opts, ",retrace=", &nextopt)) {
            opts = nextopt;
            if (strstart(opts, "dumb", &nextopt))
                vga_retrace_method = VGA_RETRACE_DUMB;
            else if (strstart(opts, "precise", &nextopt))
                vga_retrace_method = VGA_RETRACE_PRECISE;
            else goto invalid_vga;
        } else goto invalid_vga;
        opts = nextopt;
    }

    return true;
}

static DisplayType select_display(const char *p)
{
    const char *opts;
    DisplayType display = DT_DEFAULT;

    if (strstart(p, "sdl", &opts)) {
#if defined(CONFIG_SDL) || defined(USE_ANDROID_EMU)
        display = DT_SDL;
        while (*opts) {
            const char *nextopt;

            if (strstart(opts, ",frame=", &nextopt)) {
                opts = nextopt;
                if (strstart(opts, "on", &nextopt)) {
                    no_frame = 0;
                } else if (strstart(opts, "off", &nextopt)) {
                    no_frame = 1;
                } else {
                    goto invalid_sdl_args;
                }
            } else if (strstart(opts, ",alt_grab=", &nextopt)) {
                opts = nextopt;
                if (strstart(opts, "on", &nextopt)) {
                    alt_grab = 1;
                } else if (strstart(opts, "off", &nextopt)) {
                    alt_grab = 0;
                } else {
                    goto invalid_sdl_args;
                }
            } else if (strstart(opts, ",ctrl_grab=", &nextopt)) {
                opts = nextopt;
                if (strstart(opts, "on", &nextopt)) {
                    ctrl_grab = 1;
                } else if (strstart(opts, "off", &nextopt)) {
                    ctrl_grab = 0;
                } else {
                    goto invalid_sdl_args;
                }
            } else if (strstart(opts, ",window_close=", &nextopt)) {
                opts = nextopt;
                if (strstart(opts, "on", &nextopt)) {
                    no_quit = 0;
                } else if (strstart(opts, "off", &nextopt)) {
                    no_quit = 1;
                } else {
                    goto invalid_sdl_args;
                }
            } else {
            invalid_sdl_args:
                fprintf(stderr, "Invalid SDL option string: %s\n", p);
                return -1;
            }
            opts = nextopt;
        }
#else
        fprintf(stderr, "SDL support is disabled\n");
        return -1;
#endif
    } else if (strstart(p, "vnc", &opts)) {
#ifdef CONFIG_VNC
        display_remote++;

        if (*opts) {
            const char *nextopt;

            if (strstart(opts, "=", &nextopt)) {
                vnc_display = nextopt;
            }
        }
        if (!vnc_display) {
            fprintf(stderr, "VNC requires a display argument vnc=<display>\n");
            return -1;
        }
#else
        fprintf(stderr, "VNC support is disabled\n");
        return -1;
#endif
    } else if (strstart(p, "curses", &opts)) {
#ifdef CONFIG_CURSES
        display = DT_CURSES;
#else
        fprintf(stderr, "Curses support is disabled\n");
        return -1;
#endif
    } else if (strstart(p, "gtk", &opts)) {
#ifdef CONFIG_GTK
        display = DT_GTK;
        while (*opts) {
            const char *nextopt;

            if (strstart(opts, ",grab_on_hover=", &nextopt)) {
                opts = nextopt;
                if (strstart(opts, "on", &nextopt)) {
                    grab_on_hover = true;
                } else if (strstart(opts, "off", &nextopt)) {
                    grab_on_hover = false;
                } else {
                    goto invalid_gtk_args;
                }
            } else {
            invalid_gtk_args:
                fprintf(stderr, "Invalid GTK option string: %s\n", p);
                return -1;
            }
            opts = nextopt;
        }
#else
        fprintf(stderr, "GTK support is disabled\n");
        return -1;
#endif
    } else if (strstart(p, "none", &opts)) {
        display = DT_NONE;
    } else {
        fprintf(stderr, "Unknown display type: %s\n", p);
        return -1;
    }

    return display;
}

static int balloon_parse(const char *arg)
{
    QemuOpts *opts;

    if (strcmp(arg, "none") == 0) {
        return 0;
    }

    if (!strncmp(arg, "virtio", 6)) {
        if (arg[6] == ',') {
            /* have params -> parse them */
            opts = qemu_opts_parse(qemu_find_opts("device"), arg+7, 0);
            if (!opts)
                return  -1;
        } else {
            /* create empty opts */
            opts = qemu_opts_create(qemu_find_opts("device"), NULL, 0,
                                    &error_abort);
        }
        qemu_opt_set(opts, "driver", "virtio-balloon");
        return 0;
    }

    return -1;
}

char *qemu_find_file(int type, const char *name)
{
    int i;
    const char *subdir;
    char *buf;

    /* Try the name as a straight path first */
    if (access(name, R_OK) == 0) {
        trace_load_file(name, name);
        return g_strdup(name);
    }

    switch (type) {
    case QEMU_FILE_TYPE_BIOS:
        subdir = "";
        break;
    case QEMU_FILE_TYPE_KEYMAP:
        subdir = "keymaps/";
        break;
    default:
        abort();
    }

    for (i = 0; i < data_dir_idx; i++) {
        buf = g_strdup_printf("%s/%s%s", data_dir[i], subdir, name);
        if (access(buf, R_OK) == 0) {
            trace_load_file(name, buf);
            return buf;
        }
        g_free(buf);
    }
    return NULL;
}

static int device_help_func(QemuOpts *opts, void *opaque)
{
    return qdev_device_help(opts);
}

static int device_init_func(QemuOpts *opts, void *opaque)
{
    DeviceState *dev;

    dev = qdev_device_add(opts);
    if (!dev)
        return -1;
    object_unref(OBJECT(dev));
    return 0;
}

static int chardev_init_func(QemuOpts *opts, void *opaque)
{
    Error *local_err = NULL;

    qemu_chr_new_from_opts(opts, NULL, &local_err);
    if (local_err) {
        error_report("%s", error_get_pretty(local_err));
        error_free(local_err);
        return -1;
    }
    return 0;
}

#ifdef CONFIG_VIRTFS
static int fsdev_init_func(QemuOpts *opts, void *opaque)
{
    int ret;
    ret = qemu_fsdev_add(opts);

    return ret;
}
#endif

static int mon_init_func(QemuOpts *opts, void *opaque)
{
    CharDriverState *chr;
    const char *chardev;
    const char *mode;
    int flags;

    mode = qemu_opt_get(opts, "mode");
    if (mode == NULL) {
        mode = "readline";
    }
    if (strcmp(mode, "readline") == 0) {
        flags = MONITOR_USE_READLINE;
    } else if (strcmp(mode, "control") == 0) {
        flags = MONITOR_USE_CONTROL;
    } else if (strcmp(mode, "android-console") == 0) {
        flags = MONITOR_ANDROID_CONSOLE | MONITOR_USE_READLINE;
    } else {
        fprintf(stderr, "unknown monitor mode \"%s\"\n", mode);
        return 1;
    }

    if (qemu_opt_get_bool(opts, "pretty", 0))
        flags |= MONITOR_USE_PRETTY;

    if (qemu_opt_get_bool(opts, "default", 0))
        flags |= MONITOR_IS_DEFAULT;

    chardev = qemu_opt_get(opts, "chardev");
    chr = qemu_chr_find(chardev);
    if (chr == NULL) {
        fprintf(stderr, "chardev \"%s\" not found\n", chardev);
        return 1;
    }

    qemu_chr_fe_claim_no_fail(chr);
    monitor_init(chr, flags);
    return 0;
}

static bool monitor_parse(const char *optarg, const char *mode)
{
    static int monitor_device_index = 0;
    QemuOpts *opts;
    const char *p;
    char label[32];
    int def = 0;

    if (strstart(optarg, "chardev:", &p)) {
        snprintf(label, sizeof(label), "%s", p);
    } else {
        snprintf(label, sizeof(label), "compat_monitor%d",
                 monitor_device_index);
        if (monitor_device_index == 0) {
            def = 1;
        }
        opts = qemu_chr_parse_compat(label, optarg);
        if (!opts) {
            fprintf(stderr, "parse error: %s\n", optarg);
            return false;
        }
    }

    opts = qemu_opts_create(qemu_find_opts("mon"), label, 1, NULL);
    if (!opts) {
        fprintf(stderr, "duplicate chardev: %s\n", label);
        return false;
    }
    qemu_opt_set(opts, "mode", mode);
    qemu_opt_set(opts, "chardev", label);
    if (def)
        qemu_opt_set(opts, "default", "on");
    monitor_device_index++;

    return true;
}

struct device_config {
    enum {
        DEV_USB,       /* -usbdevice     */
        DEV_BT,        /* -bt            */
        DEV_SERIAL,    /* -serial        */
        DEV_PARALLEL,  /* -parallel      */
        DEV_VIRTCON,   /* -virtioconsole */
        DEV_DEBUGCON,  /* -debugcon */
        DEV_GDB,       /* -gdb, -s */
        DEV_SCLP,      /* s390 sclp */
    } type;
    const char *cmdline;
    Location loc;
    QTAILQ_ENTRY(device_config) next;
};

static QTAILQ_HEAD(, device_config) device_configs =
    QTAILQ_HEAD_INITIALIZER(device_configs);

static void add_device_config(int type, const char *cmdline)
{
    struct device_config *conf;

    conf = g_malloc0(sizeof(*conf));
    conf->type = type;
    conf->cmdline = cmdline;
    loc_save(&conf->loc);
    QTAILQ_INSERT_TAIL(&device_configs, conf, next);
}

static int foreach_device_config(int type, int (*func)(const char *cmdline))
{
    struct device_config *conf;
    int rc;

    QTAILQ_FOREACH(conf, &device_configs, next) {
        if (conf->type != type)
            continue;
        loc_push_restore(&conf->loc);
        rc = func(conf->cmdline);
        loc_pop(&conf->loc);
        if (rc) {
            return rc;
        }
    }
    return 0;
}

static int serial_parse(const char *devname)
{
    static int index = 0;
    char label[32];

    if (strcmp(devname, "none") == 0)
        return 0;
    if (index == MAX_SERIAL_PORTS) {
        fprintf(stderr, "qemu: too many serial ports\n");
        return -1;
    }
    snprintf(label, sizeof(label), "serial%d", index);
    serial_hds[index] = qemu_chr_new(label, devname, NULL);
    if (!serial_hds[index]) {
        fprintf(stderr, "qemu: could not connect serial device"
                " to character backend '%s'\n", devname);
        return -1;
    }
    index++;
    return 0;
}

static int parallel_parse(const char *devname)
{
    static int index = 0;
    char label[32];

    if (strcmp(devname, "none") == 0)
        return 0;
    if (index == MAX_PARALLEL_PORTS) {
        fprintf(stderr, "qemu: too many parallel ports\n");
        return -1;
    }
    snprintf(label, sizeof(label), "parallel%d", index);
    parallel_hds[index] = qemu_chr_new(label, devname, NULL);
    if (!parallel_hds[index]) {
        fprintf(stderr, "qemu: could not connect parallel device"
                " to character backend '%s'\n", devname);
        return -1;
    }
    index++;
    return 0;
}

static int virtcon_parse(const char *devname)
{
    QemuOptsList *device = qemu_find_opts("device");
    static int index = 0;
    char label[32];
    QemuOpts *bus_opts, *dev_opts;

    if (strcmp(devname, "none") == 0)
        return 0;
    if (index == MAX_VIRTIO_CONSOLES) {
        fprintf(stderr, "qemu: too many virtio consoles\n");
        return -1;
    }

    bus_opts = qemu_opts_create(device, NULL, 0, &error_abort);
    if (arch_type == QEMU_ARCH_S390X) {
        qemu_opt_set(bus_opts, "driver", "virtio-serial-s390");
    } else {
        qemu_opt_set(bus_opts, "driver", "virtio-serial-pci");
    }

    dev_opts = qemu_opts_create(device, NULL, 0, &error_abort);
    qemu_opt_set(dev_opts, "driver", "virtconsole");

    snprintf(label, sizeof(label), "virtcon%d", index);
    virtcon_hds[index] = qemu_chr_new(label, devname, NULL);
    if (!virtcon_hds[index]) {
        fprintf(stderr, "qemu: could not connect virtio console"
                " to character backend '%s'\n", devname);
        return -1;
    }
    qemu_opt_set(dev_opts, "chardev", label);

    index++;
    return 0;
}

static int sclp_parse(const char *devname)
{
    QemuOptsList *device = qemu_find_opts("device");
    static int index = 0;
    char label[32];
    QemuOpts *dev_opts;

    if (strcmp(devname, "none") == 0) {
        return 0;
    }
    if (index == MAX_SCLP_CONSOLES) {
        fprintf(stderr, "qemu: too many sclp consoles\n");
        return -1;
    }

    assert(arch_type == QEMU_ARCH_S390X);

    dev_opts = qemu_opts_create(device, NULL, 0, NULL);
    qemu_opt_set(dev_opts, "driver", "sclpconsole");

    snprintf(label, sizeof(label), "sclpcon%d", index);
    sclp_hds[index] = qemu_chr_new(label, devname, NULL);
    if (!sclp_hds[index]) {
        fprintf(stderr, "qemu: could not connect sclp console"
                " to character backend '%s'\n", devname);
        return -1;
    }
    qemu_opt_set(dev_opts, "chardev", label);

    index++;
    return 0;
}

static int debugcon_parse(const char *devname)
{
    QemuOpts *opts;

    if (!qemu_chr_new("debugcon", devname, NULL)) {
        return -1;
    }
    opts = qemu_opts_create(qemu_find_opts("device"), "debugcon", 1, NULL);
    if (!opts) {
        fprintf(stderr, "qemu: already have a debugcon device\n");
        return -1;
    }
    qemu_opt_set(opts, "driver", "isa-debugcon");
    qemu_opt_set(opts, "chardev", "debugcon");
    return 0;
}

static gint machine_class_cmp(gconstpointer a, gconstpointer b)
{
    const MachineClass *mc1 = a, *mc2 = b;
    int res;

    if (mc1->family == NULL) {
        if (mc2->family == NULL) {
            /* Compare standalone machine types against each other; they sort
             * in increasing order.
             */
            return strcmp(object_class_get_name(OBJECT_CLASS(mc1)),
                          object_class_get_name(OBJECT_CLASS(mc2)));
        }

        /* Standalone machine types sort after families. */
        return 1;
    }

    if (mc2->family == NULL) {
        /* Families sort before standalone machine types. */
        return -1;
    }

    /* Families sort between each other alphabetically increasingly. */
    res = strcmp(mc1->family, mc2->family);
    if (res != 0) {
        return res;
    }

    /* Within the same family, machine types sort in decreasing order. */
    return strcmp(object_class_get_name(OBJECT_CLASS(mc2)),
                  object_class_get_name(OBJECT_CLASS(mc1)));
}

static MachineClass *machine_parse(const char *name)
{
    MachineClass *mc = NULL;
    GSList *el, *machines = object_class_get_list(TYPE_MACHINE, false);

    if (name) {
        mc = find_machine(name);
    }
    if (mc) {
        return mc;
    }
    if (name && !is_help_option(name)) {
        error_report("Unsupported machine type");
        error_printf("Use -machine help to list supported machines!\n");
    } else {
        printf("Supported machines are:\n");
        machines = g_slist_sort(machines, machine_class_cmp);
        for (el = machines; el; el = el->next) {
            MachineClass *mc = el->data;
            if (mc->alias) {
                printf("%-20s %s (alias of %s)\n", mc->alias, mc->desc, mc->name);
            }
            printf("%-20s %s%s\n", mc->name, mc->desc,
                   mc->is_default ? " (default)" : "");
        }
    }

    g_slist_free(machines);
    return NULL;
}

void qemu_add_exit_notifier(Notifier *notify)
{
    notifier_list_add(&exit_notifiers, notify);
}

void qemu_remove_exit_notifier(Notifier *notify)
{
    notifier_remove(notify);
}

static void qemu_run_exit_notifiers(void)
{
    notifier_list_notify(&exit_notifiers, NULL);
}

void qemu_add_machine_init_done_notifier(Notifier *notify)
{
    notifier_list_add(&machine_init_done_notifiers, notify);
}

static void qemu_run_machine_init_done_notifiers(void)
{
    notifier_list_notify(&machine_init_done_notifiers, NULL);
}

static const QEMUOption *lookup_opt(int argc, const char **argv,
                                    const char **poptarg, int *poptind)
{
    const QEMUOption *popt;
    int optind = *poptind;
    const char *r = argv[optind];
    const char *optarg;

    loc_set_cmdline(argv, optind, 1);
    optind++;
    /* Treat --foo the same as -foo.  */
    if (r[1] == '-')
        r++;
    popt = qemu_options;
    for(;;) {
        if (!popt->name) {
            error_report("invalid option");
            return NULL;
        }
        if (!strcmp(popt->name, r + 1))
            break;
        popt++;
    }
    if (popt->flags & HAS_ARG) {
        if (optind >= argc) {
            error_report("requires an argument");
            return NULL;
        }
        optarg = argv[optind++];
        loc_set_cmdline(argv, optind - 2, 2);
    } else {
        optarg = NULL;
    }

    *poptarg = optarg;
    *poptind = optind;

    return popt;
}

static gpointer malloc_and_trace(gsize n_bytes)
{
    void *ptr = malloc(n_bytes);
    trace_g_malloc(n_bytes, ptr);
    return ptr;
}

static gpointer realloc_and_trace(gpointer mem, gsize n_bytes)
{
    void *ptr = realloc(mem, n_bytes);
    trace_g_realloc(mem, n_bytes, ptr);
    return ptr;
}

static void free_and_trace(gpointer mem)
{
    trace_g_free(mem);
    free(mem);
}

static int machine_set_property(const char *name, const char *value,
                                void *opaque)
{
    Object *obj = OBJECT(opaque);
    StringInputVisitor *siv;
    Error *local_err = NULL;
    char *c, *qom_name;

    if (strcmp(name, "type") == 0) {
        return 0;
    }

    qom_name = g_strdup(name);
    c = qom_name;
    while (*c++) {
        if (*c == '_') {
            *c = '-';
        }
    }

    siv = string_input_visitor_new(value);
    object_property_set(obj, string_input_get_visitor(siv), qom_name, &local_err);
    string_input_visitor_cleanup(siv);
    g_free(qom_name);

    if (local_err) {
        qerror_report_err(local_err);
        error_free(local_err);
        return -1;
    }

    return 0;
}

static int object_create(QemuOpts *opts, void *opaque)
{
    Error *err = NULL;
    char *type = NULL;
    char *id = NULL;
    void *dummy = NULL;
    OptsVisitor *ov;
    QDict *pdict;

    ov = opts_visitor_new(opts);
    pdict = qemu_opts_to_qdict(opts, NULL);

    visit_start_struct(opts_get_visitor(ov), &dummy, NULL, NULL, 0, &err);
    if (err) {
        goto out;
    }

    qdict_del(pdict, "qom-type");
    visit_type_str(opts_get_visitor(ov), &type, "qom-type", &err);
    if (err) {
        goto out;
    }

    qdict_del(pdict, "id");
    visit_type_str(opts_get_visitor(ov), &id, "id", &err);
    if (err) {
        goto out;
    }

    object_add(type, id, pdict, opts_get_visitor(ov), &err);
    if (err) {
        goto out;
    }
    visit_end_struct(opts_get_visitor(ov), &err);
    if (err) {
        qmp_object_del(id, NULL);
    }

out:
    opts_visitor_cleanup(ov);

    QDECREF(pdict);
    g_free(id);
    g_free(type);
    g_free(dummy);
    if (err) {
        qerror_report_err(err);
        error_free(err);
        return -1;
    }
    return 0;
}

#if !defined(CONFIG_ANDROID) || !defined(USE_ANDROID_EMU)
// We don't use the AndroidEmu library in the original qemu2 build,
// so let's return their main function back
#define run_qemu_main main
#endif

#if defined(USE_ANDROID_EMU)

static int is_opengl_alive = 1;

static void android_check_for_updates()
{
    char configPath[MAX_PATH];
    bufprint_config_path(configPath, configPath + sizeof(configPath));
    android_checkForUpdates(configPath, QEMU_CORE_VERSION);
}

static void android_init_metrics()
{
    char path[MAX_PATH], *pathend=path, *bufend=pathend+sizeof(path);
    AndroidMetrics metrics;

    if (!android_studio_get_optins()) {
        return;
    }

    pathend = bufprint_avd_home_path(path, bufend);
    if (pathend >= bufend || !androidMetrics_moduleInit(path))
    {
        printf("Failed to initialize metrics reporting.\n");
        return;
    }

    androidMetrics_init(&metrics);
    ANDROID_METRICS_STRASSIGN(metrics.emulator_version,
                              EMULATOR_VERSION_STRING);
    ANDROID_METRICS_STRASSIGN(metrics.core_version,
                              QEMU_CORE_VERSION);

    ANDROID_METRICS_STRASSIGN(metrics.host_os_type, get_host_os_type());
    ANDROID_METRICS_STRASSIGN(metrics.guest_arch, android_hw->hw_cpu_arch);
    metrics.guest_api_level = avdInfo_getApiLevel(android_avdInfo);
    metrics.guest_gpu_enabled = android_hw->hw_gpu_enabled;
    if (android_hw->hw_gpu_enabled) {
        free(metrics.guest_gl_vendor);
        metrics.guest_gl_vendor = NULL;
        free(metrics.guest_gl_renderer);
        metrics.guest_gl_renderer = NULL;
        free(metrics.guest_gl_version);
        metrics.guest_gl_version = NULL;
        // This call is only sensible after |android_startOpenglesRenderer| has
        // been called.
        android_getOpenglesHardwareStrings(&metrics.guest_gl_vendor,
                                           &metrics.guest_gl_renderer,
                                           &metrics.guest_gl_version);
    }

    // Tell the metrics the host GPU information
    emugl_host_gpu_prop_list gpu_props = emuglConfig_get_host_gpu_props();
    androidMetrics_populateGpuProps(&metrics, &gpu_props);
    free_emugl_host_gpu_props(gpu_props);

    metrics.opengl_alive = is_opengl_alive;
    androidMetrics_write(&metrics);
    androidMetrics_fini(&metrics);

    async((async_function_t)androidMetrics_tryReportAll);

    androidMetrics_keepAlive(looper_getForThread(), android_base_port);
}

static void android_teardown_metrics()
{
    // NB: It is safe to cleanup metrics reporting even if we never initialized
    // it.
    androidMetrics_seal();
    androidMetrics_moduleFini();
}

static bool android_reporting_setup(void)
{
    android_init_metrics();
    if (!is_opengl_alive) {
        derror("Could not initialize OpenglES emulation, "
               "use '-gpu off' to disable it.");
        return false;
    }

    android_check_for_updates();
    return true;
}

static void android_reporting_teardown(void)
{
    android_teardown_metrics();
}

#else

static bool android_reporting_setup(void)
{
    return true;
}

static void android_reporting_teardown(void)
{
}

#endif

int run_qemu_main(int argc, const char **argv)
{
    int i;
    int snapshot, linux_boot;
    const char *initrd_filename;
    const char *kernel_filename, *kernel_cmdline;
    const char *boot_order;
    DisplayState *ds;
    int cyls, heads, secs, translation;
    QemuOpts *hda_opts = NULL, *opts, *machine_opts, *icount_opts = NULL;
    QemuOptsList *olist;
    int optind;
    const char *optarg;
    const char *loadvm = NULL;
    MachineClass *machine_class;
    const char *cpu_model;
    const char *vga_model = NULL;
    const char *qtest_chrdev = NULL;
    const char *qtest_log = NULL;
    const char *pid_file = NULL;
    const char *incoming = NULL;
#ifdef CONFIG_VNC
    int show_vnc_port = 0;
#endif
    bool defconfig = true;
    bool userconfig = true;
    const char *log_mask = NULL;
    const char *log_file = NULL;
    GMemVTable mem_trace = {
        .malloc = malloc_and_trace,
        .realloc = realloc_and_trace,
        .free = free_and_trace,
    };
    const char *trace_events = NULL;
    const char *trace_file = NULL;
    const ram_addr_t default_ram_size = (ram_addr_t)DEFAULT_RAM_SIZE *
                                        1024 * 1024;
    ram_addr_t maxram_size = default_ram_size;
    uint64_t ram_slots = 0;
    FILE *vmstate_dump_file = NULL;
    Error *main_loop_err = NULL;

    atexit(qemu_run_exit_notifiers);
    error_set_progname(argv[0]);
    qemu_init_exec_dir(argv[0]);

    g_mem_set_vtable(&mem_trace);

    module_call_init(MODULE_INIT_QOM);

    qemu_add_opts(&qemu_drive_opts);
    qemu_add_drive_opts(&qemu_legacy_drive_opts);
    qemu_add_drive_opts(&qemu_common_drive_opts);
    qemu_add_drive_opts(&qemu_drive_opts);
    qemu_add_opts(&qemu_chardev_opts);
    qemu_add_opts(&qemu_device_opts);
    qemu_add_opts(&qemu_netdev_opts);
    qemu_add_opts(&qemu_net_opts);
    qemu_add_opts(&qemu_rtc_opts);
    qemu_add_opts(&qemu_global_opts);
    qemu_add_opts(&qemu_mon_opts);
    qemu_add_opts(&qemu_trace_opts);
    qemu_add_opts(&qemu_option_rom_opts);
    qemu_add_opts(&qemu_machine_opts);
    qemu_add_opts(&qemu_mem_opts);
    qemu_add_opts(&qemu_smp_opts);
    qemu_add_opts(&qemu_boot_opts);
    qemu_add_opts(&qemu_sandbox_opts);
    qemu_add_opts(&qemu_add_fd_opts);
    qemu_add_opts(&qemu_object_opts);
    qemu_add_opts(&qemu_tpmdev_opts);
    qemu_add_opts(&qemu_realtime_opts);
    qemu_add_opts(&qemu_msg_opts);
    qemu_add_opts(&qemu_name_opts);
    qemu_add_opts(&qemu_numa_opts);
    qemu_add_opts(&qemu_icount_opts);

    runstate_init();

    rtc_clock = QEMU_CLOCK_HOST;

    QLIST_INIT (&vm_change_state_head);
    os_setup_early_signal_handling();

    module_call_init(MODULE_INIT_MACHINE);
    machine_class = find_default_machine();
    cpu_model = NULL;
    ram_size = default_ram_size;
    snapshot = 0;
    cyls = heads = secs = 0;
    translation = BIOS_ATA_TRANSLATION_AUTO;

    for (i = 0; i < MAX_NODES; i++) {
        numa_info[i].node_mem = 0;
        numa_info[i].present = false;
        bitmap_zero(numa_info[i].node_cpu, MAX_CPUMASK_BITS);
    }

    nb_numa_nodes = 0;
    max_numa_nodeid = 0;
    nb_nics = 0;

    bdrv_init_with_whitelist();

    autostart = 1;

    /* first pass of option parsing */
    optind = 1;
    while (optind < argc) {
        if (argv[optind][0] != '-') {
            /* disk image */
            optind++;
        } else {
            const QEMUOption *popt;

            popt = lookup_opt(argc, argv, &optarg, &optind);
            if (!popt) {
                return 1;
            }
            switch (popt->index) {
            case QEMU_OPTION_nodefconfig:
                defconfig = false;
                break;
            case QEMU_OPTION_nouserconfig:
                userconfig = false;
                break;
            }
        }
    }

    if (defconfig) {
        int ret;
        ret = qemu_read_default_config_files(userconfig);
        if (ret < 0) {
            return 1;
        }
    }

    /* second pass of option parsing */
    optind = 1;
    for(;;) {
        if (optind >= argc)
            break;
        if (argv[optind][0] != '-') {
	    hda_opts = drive_add(IF_DEFAULT, 0, argv[optind++], HD_OPTS);
        } else {
            const QEMUOption *popt;

            popt = lookup_opt(argc, argv, &optarg, &optind);
            if (!popt) {
                return 1;
            }
            if (!(popt->arch_mask & arch_type)) {
                printf("Option %s not supported for this target\n", popt->name);
                return 1;
            }
            switch(popt->index) {
            case QEMU_OPTION_M:
                machine_class = machine_parse(optarg);
                if (!machine_class) {
                    return 1;
                }
                break;
            case QEMU_OPTION_no_kvm_irqchip: {
                olist = qemu_find_opts("machine");
                qemu_opts_parse(olist, "kernel_irqchip=off", 0);
                break;
            }
            case QEMU_OPTION_cpu:
                /* hw initialization will check this */
                cpu_model = optarg;
                break;
            case QEMU_OPTION_hda:
                {
                    char buf[256];
                    if (cyls == 0)
                        snprintf(buf, sizeof(buf), "%s", HD_OPTS);
                    else
                        snprintf(buf, sizeof(buf),
                                 "%s,cyls=%d,heads=%d,secs=%d%s",
                                 HD_OPTS , cyls, heads, secs,
                                 translation == BIOS_ATA_TRANSLATION_LBA ?
                                 ",trans=lba" :
                                 translation == BIOS_ATA_TRANSLATION_NONE ?
                                 ",trans=none" : "");
                    drive_add(IF_DEFAULT, 0, optarg, buf);
                    break;
                }
            case QEMU_OPTION_hdb:
            case QEMU_OPTION_hdc:
            case QEMU_OPTION_hdd:
                drive_add(IF_DEFAULT, popt->index - QEMU_OPTION_hda, optarg,
                          HD_OPTS);
                break;
            case QEMU_OPTION_drive:
                if (drive_def(optarg) == NULL) {
                    return 1;
                }
	        break;
            case QEMU_OPTION_set:
                if (qemu_set_option(optarg) != 0)
                    return 1;
	        break;
            case QEMU_OPTION_global:
                if (qemu_global_option(optarg) != 0)
                    return 1;
	        break;
            case QEMU_OPTION_mtdblock:
                drive_add(IF_MTD, -1, optarg, MTD_OPTS);
                break;
            case QEMU_OPTION_sd:
                drive_add(IF_SD, -1, optarg, SD_OPTS);
                break;
            case QEMU_OPTION_pflash:
                drive_add(IF_PFLASH, -1, optarg, PFLASH_OPTS);
                break;
            case QEMU_OPTION_snapshot:
                snapshot = 1;
                break;
            case QEMU_OPTION_hdachs:
                {
                    const char *p;
                    p = optarg;
                    cyls = strtol(p, (char **)&p, 0);
                    if (cyls < 1 || cyls > 16383)
                        goto chs_fail;
                    if (*p != ',')
                        goto chs_fail;
                    p++;
                    heads = strtol(p, (char **)&p, 0);
                    if (heads < 1 || heads > 16)
                        goto chs_fail;
                    if (*p != ',')
                        goto chs_fail;
                    p++;
                    secs = strtol(p, (char **)&p, 0);
                    if (secs < 1 || secs > 63)
                        goto chs_fail;
                    if (*p == ',') {
                        p++;
                        if (!strcmp(p, "large")) {
                            translation = BIOS_ATA_TRANSLATION_LARGE;
                        } else if (!strcmp(p, "rechs")) {
                            translation = BIOS_ATA_TRANSLATION_RECHS;
                        } else if (!strcmp(p, "none")) {
                            translation = BIOS_ATA_TRANSLATION_NONE;
                        } else if (!strcmp(p, "lba")) {
                            translation = BIOS_ATA_TRANSLATION_LBA;
                        } else if (!strcmp(p, "auto")) {
                            translation = BIOS_ATA_TRANSLATION_AUTO;
                        } else {
                            goto chs_fail;
                        }
                    } else if (*p != '\0') {
                    chs_fail:
                        fprintf(stderr, "qemu: invalid physical CHS format\n");
                        return 1;
                    }
		    if (hda_opts != NULL) {
                        char num[16];
                        snprintf(num, sizeof(num), "%d", cyls);
                        qemu_opt_set(hda_opts, "cyls", num);
                        snprintf(num, sizeof(num), "%d", heads);
                        qemu_opt_set(hda_opts, "heads", num);
                        snprintf(num, sizeof(num), "%d", secs);
                        qemu_opt_set(hda_opts, "secs", num);
                        if (translation == BIOS_ATA_TRANSLATION_LARGE) {
                            qemu_opt_set(hda_opts, "trans", "large");
                        } else if (translation == BIOS_ATA_TRANSLATION_RECHS) {
                            qemu_opt_set(hda_opts, "trans", "rechs");
                        } else if (translation == BIOS_ATA_TRANSLATION_LBA) {
                            qemu_opt_set(hda_opts, "trans", "lba");
                        } else if (translation == BIOS_ATA_TRANSLATION_NONE) {
                            qemu_opt_set(hda_opts, "trans", "none");
                        }
                    }
                }
                break;
            case QEMU_OPTION_numa:
                opts = qemu_opts_parse(qemu_find_opts("numa"), optarg, 1);
                if (!opts) {
                    return 1;
                }
                break;
            case QEMU_OPTION_display:
                display_type = select_display(optarg);
                if (display_type < 0) {
                    return 1;
                }
                break;
            case QEMU_OPTION_nographic:
                display_type = DT_NOGRAPHIC;
                break;
            case QEMU_OPTION_curses:
#ifdef CONFIG_CURSES
                display_type = DT_CURSES;
#else
                fprintf(stderr, "Curses support is disabled\n");
                return 1;
#endif
                break;
            case QEMU_OPTION_portrait:
                graphic_rotate = 90;
                break;
            case QEMU_OPTION_rotate:
                graphic_rotate = strtol(optarg, (char **) &optarg, 10);
                if (graphic_rotate != 0 && graphic_rotate != 90 &&
                    graphic_rotate != 180 && graphic_rotate != 270) {
                    fprintf(stderr,
                        "qemu: only 90, 180, 270 deg rotation is available\n");
                    return 1;
                }
                break;
            case QEMU_OPTION_kernel:
                qemu_opts_set(qemu_find_opts("machine"), 0, "kernel", optarg);
                break;
            case QEMU_OPTION_initrd:
                qemu_opts_set(qemu_find_opts("machine"), 0, "initrd", optarg);
                break;
            case QEMU_OPTION_append:
                qemu_opts_set(qemu_find_opts("machine"), 0, "append", optarg);
                break;
            case QEMU_OPTION_dtb:
                qemu_opts_set(qemu_find_opts("machine"), 0, "dtb", optarg);
                break;
            case QEMU_OPTION_cdrom:
                drive_add(IF_DEFAULT, 2, optarg, CDROM_OPTS);
                break;
            case QEMU_OPTION_boot:
                opts = qemu_opts_parse(qemu_find_opts("boot-opts"), optarg, 1);
                if (!opts) {
                    return 1;
                }
                break;
            case QEMU_OPTION_fda:
            case QEMU_OPTION_fdb:
                drive_add(IF_FLOPPY, popt->index - QEMU_OPTION_fda,
                          optarg, FD_OPTS);
                break;
            case QEMU_OPTION_no_fd_bootchk:
                fd_bootchk = 0;
                break;
            case QEMU_OPTION_netdev:
                if (net_client_parse(qemu_find_opts("netdev"), optarg) == -1) {
                    return 1;
                }
                break;
            case QEMU_OPTION_net:
                if (net_client_parse(qemu_find_opts("net"), optarg) == -1) {
                    return 1;
                }
                break;
#ifdef CONFIG_LIBISCSI
            case QEMU_OPTION_iscsi:
                opts = qemu_opts_parse(qemu_find_opts("iscsi"), optarg, 0);
                if (!opts) {
                    return 1;
                }
                break;
#endif
#ifdef CONFIG_SLIRP
            case QEMU_OPTION_tftp:
                legacy_tftp_prefix = optarg;
                break;
            case QEMU_OPTION_bootp:
                legacy_bootp_filename = optarg;
                break;
            case QEMU_OPTION_redir:
                if (net_slirp_redir(optarg) < 0)
                    return 1;
                break;
#endif
            case QEMU_OPTION_bt:
                add_device_config(DEV_BT, optarg);
                break;
            case QEMU_OPTION_audio_help:
                AUD_help ();
                return (0);
                break;
            case QEMU_OPTION_soundhw:
                select_soundhw (optarg);
                break;
            case QEMU_OPTION_h:
                help();
                return 0;
                break;
            case QEMU_OPTION_version:
                version();
                return 0;
                break;
            case QEMU_OPTION_m: {
                uint64_t sz;
                const char *mem_str;
                const char *maxmem_str, *slots_str;

                opts = qemu_opts_parse(qemu_find_opts("memory"),
                                       optarg, 1);
                if (!opts) {
                    return EXIT_FAILURE;
                }

                mem_str = qemu_opt_get(opts, "size");
                if (!mem_str) {
                    error_report("invalid -m option, missing 'size' option");
                    return EXIT_FAILURE;
                }
                if (!*mem_str) {
                    error_report("missing 'size' option value");
                    return EXIT_FAILURE;
                }

                sz = qemu_opt_get_size(opts, "size", ram_size);

                /* Fix up legacy suffix-less format */
                if (g_ascii_isdigit(mem_str[strlen(mem_str) - 1])) {
                    uint64_t overflow_check = sz;

                    sz <<= 20;
                    if ((sz >> 20) != overflow_check) {
                        error_report("too large 'size' option value");
                        return EXIT_FAILURE;
                    }
                }

                /* backward compatibility behaviour for case "-m 0" */
                if (sz == 0) {
                    sz = default_ram_size;
                }

                sz = QEMU_ALIGN_UP(sz, 8192);
                ram_size = sz;
                if (ram_size != sz) {
                    error_report("ram size too large");
                    return EXIT_FAILURE;
                }
                maxram_size = ram_size;

                maxmem_str = qemu_opt_get(opts, "maxmem");
                slots_str = qemu_opt_get(opts, "slots");
                if (maxmem_str && slots_str) {
                    uint64_t slots;

                    sz = qemu_opt_get_size(opts, "maxmem", 0);
                    if (sz < ram_size) {
                        error_report("invalid -m option value: maxmem "
                                "(0x%" PRIx64 ") <= initial memory (0x"
                                RAM_ADDR_FMT ")", sz, ram_size);
                        return EXIT_FAILURE;
                    }

                    slots = qemu_opt_get_number(opts, "slots", 0);
                    if ((sz > ram_size) && !slots) {
                        error_report("invalid -m option value: maxmem "
                                "(0x%" PRIx64 ") more than initial memory (0x"
                                RAM_ADDR_FMT ") but no hotplug slots where "
                                "specified", sz, ram_size);
                        return EXIT_FAILURE;
                    }

                    if ((sz <= ram_size) && slots) {
                        error_report("invalid -m option value:  %"
                                PRIu64 " hotplug slots where specified but "
                                "maxmem (0x%" PRIx64 ") <= initial memory (0x"
                                RAM_ADDR_FMT ")", slots, sz, ram_size);
                        return EXIT_FAILURE;
                    }
                    maxram_size = sz;
                    ram_slots = slots;
                } else if ((!maxmem_str && slots_str) ||
                           (maxmem_str && !slots_str)) {
                    error_report("invalid -m option value: missing "
                            "'%s' option", slots_str ? "maxmem" : "slots");
                    return EXIT_FAILURE;
                }
                break;
            }
#ifdef CONFIG_TPM
            case QEMU_OPTION_tpmdev:
                if (tpm_config_parse(qemu_find_opts("tpmdev"), optarg) < 0) {
                    return 1;
                }
                break;
#endif
            case QEMU_OPTION_mempath:
                mem_path = optarg;
                break;
            case QEMU_OPTION_mem_prealloc:
                mem_prealloc = 1;
                break;
            case QEMU_OPTION_d:
                log_mask = optarg;
                break;
            case QEMU_OPTION_D:
                log_file = optarg;
                break;
            case QEMU_OPTION_s:
                add_device_config(DEV_GDB, "tcp::" DEFAULT_GDBSTUB_PORT);
                break;
            case QEMU_OPTION_gdb:
                add_device_config(DEV_GDB, optarg);
                break;
            case QEMU_OPTION_L:
                if (data_dir_idx < ARRAY_SIZE(data_dir)) {
                    data_dir[data_dir_idx++] = optarg;
                }
                break;
            case QEMU_OPTION_bios:
                qemu_opts_set(qemu_find_opts("machine"), 0, "firmware", optarg);
                break;
            case QEMU_OPTION_singlestep:
                singlestep = 1;
                break;
            case QEMU_OPTION_S:
                autostart = 0;
                break;
	    case QEMU_OPTION_k:
		keyboard_layout = optarg;
		break;
            case QEMU_OPTION_localtime:
                rtc_utc = 0;
                break;
            case QEMU_OPTION_vga:
                vga_model = optarg;
                default_vga = 0;
                break;
            case QEMU_OPTION_g:
                {
                    const char *p;
                    int w, h, depth;
                    p = optarg;
                    w = strtol(p, (char **)&p, 10);
                    if (w <= 0) {
                    graphic_error:
                        fprintf(stderr, "qemu: invalid resolution or depth\n");
                        return 1;
                    }
                    if (*p != 'x')
                        goto graphic_error;
                    p++;
                    h = strtol(p, (char **)&p, 10);
                    if (h <= 0)
                        goto graphic_error;
                    if (*p == 'x') {
                        p++;
                        depth = strtol(p, (char **)&p, 10);
                        if (depth != 8 && depth != 15 && depth != 16 &&
                            depth != 24 && depth != 32)
                            goto graphic_error;
                    } else if (*p == '\0') {
                        depth = graphic_depth;
                    } else {
                        goto graphic_error;
                    }

                    graphic_width = w;
                    graphic_height = h;
                    graphic_depth = depth;
                }
                break;
            case QEMU_OPTION_echr:
                {
                    char *r;
                    term_escape_char = strtol(optarg, &r, 0);
                    if (r == optarg)
                        printf("Bad argument to echr\n");
                    break;
                }
            case QEMU_OPTION_monitor:
                default_monitor = 0;
                if (strncmp(optarg, "none", 4)) {
                    if (!monitor_parse(optarg, "readline")) {
                        return 1;
                    }
                }
                break;
            case QEMU_OPTION_qmp:
                if (!monitor_parse(optarg, "control")) {
                    return 1;
                }
                default_monitor = 0;
                break;
            case QEMU_OPTION_mon:
                opts = qemu_opts_parse(qemu_find_opts("mon"), optarg, 1);
                if (!opts) {
                    return 1;
                }
                default_monitor = 0;
                break;
            case QEMU_OPTION_chardev:
                opts = qemu_opts_parse(qemu_find_opts("chardev"), optarg, 1);
                if (!opts) {
                    return 1;
                }
                break;
            case QEMU_OPTION_fsdev:
                olist = qemu_find_opts("fsdev");
                if (!olist) {
                    fprintf(stderr, "fsdev is not supported by this qemu build.\n");
                    return 1;
                }
                opts = qemu_opts_parse(olist, optarg, 1);
                if (!opts) {
                    return 1;
                }
                break;
            case QEMU_OPTION_virtfs: {
                QemuOpts *fsdev;
                QemuOpts *device;
                const char *writeout, *sock_fd, *socket;

                olist = qemu_find_opts("virtfs");
                if (!olist) {
                    fprintf(stderr, "virtfs is not supported by this qemu build.\n");
                    return 1;
                }
                opts = qemu_opts_parse(olist, optarg, 1);
                if (!opts) {
                    return 1;
                }

                if (qemu_opt_get(opts, "fsdriver") == NULL ||
                    qemu_opt_get(opts, "mount_tag") == NULL) {
                    fprintf(stderr, "Usage: -virtfs fsdriver,mount_tag=tag.\n");
                    return 1;
                }
                fsdev = qemu_opts_create(qemu_find_opts("fsdev"),
                                         qemu_opt_get(opts, "mount_tag"),
                                         1, NULL);
                if (!fsdev) {
                    fprintf(stderr, "duplicate fsdev id: %s\n",
                            qemu_opt_get(opts, "mount_tag"));
                    return 1;
                }

                writeout = qemu_opt_get(opts, "writeout");
                if (writeout) {
#ifdef CONFIG_SYNC_FILE_RANGE
                    qemu_opt_set(fsdev, "writeout", writeout);
#else
                    fprintf(stderr, "writeout=immediate not supported on "
                            "this platform\n");
                    return 1;
#endif
                }
                qemu_opt_set(fsdev, "fsdriver", qemu_opt_get(opts, "fsdriver"));
                qemu_opt_set(fsdev, "path", qemu_opt_get(opts, "path"));
                qemu_opt_set(fsdev, "security_model",
                             qemu_opt_get(opts, "security_model"));
                socket = qemu_opt_get(opts, "socket");
                if (socket) {
                    qemu_opt_set(fsdev, "socket", socket);
                }
                sock_fd = qemu_opt_get(opts, "sock_fd");
                if (sock_fd) {
                    qemu_opt_set(fsdev, "sock_fd", sock_fd);
                }

                qemu_opt_set_bool(fsdev, "readonly",
                                qemu_opt_get_bool(opts, "readonly", 0));
                device = qemu_opts_create(qemu_find_opts("device"), NULL, 0,
                                          &error_abort);
                qemu_opt_set(device, "driver", "virtio-9p-pci");
                qemu_opt_set(device, "fsdev",
                             qemu_opt_get(opts, "mount_tag"));
                qemu_opt_set(device, "mount_tag",
                             qemu_opt_get(opts, "mount_tag"));
                break;
            }
            case QEMU_OPTION_virtfs_synth: {
                QemuOpts *fsdev;
                QemuOpts *device;

                fsdev = qemu_opts_create(qemu_find_opts("fsdev"), "v_synth",
                                         1, NULL);
                if (!fsdev) {
                    fprintf(stderr, "duplicate option: %s\n", "virtfs_synth");
                    return 1;
                }
                qemu_opt_set(fsdev, "fsdriver", "synth");

                device = qemu_opts_create(qemu_find_opts("device"), NULL, 0,
                                          &error_abort);
                qemu_opt_set(device, "driver", "virtio-9p-pci");
                qemu_opt_set(device, "fsdev", "v_synth");
                qemu_opt_set(device, "mount_tag", "v_synth");
                break;
            }
            case QEMU_OPTION_serial:
                add_device_config(DEV_SERIAL, optarg);
                default_serial = 0;
                if (strncmp(optarg, "mon:", 4) == 0) {
                    default_monitor = 0;
                }
                break;
            case QEMU_OPTION_watchdog:
                if (watchdog) {
                    fprintf(stderr,
                            "qemu: only one watchdog option may be given\n");
                    return 1;
                }
                watchdog = optarg;
                break;
            case QEMU_OPTION_watchdog_action:
                if (select_watchdog_action(optarg) == -1) {
                    fprintf(stderr, "Unknown -watchdog-action parameter\n");
                    return 1;
                }
                break;
            case QEMU_OPTION_virtiocon:
                add_device_config(DEV_VIRTCON, optarg);
                default_virtcon = 0;
                if (strncmp(optarg, "mon:", 4) == 0) {
                    default_monitor = 0;
                }
                break;
            case QEMU_OPTION_parallel:
                add_device_config(DEV_PARALLEL, optarg);
                default_parallel = 0;
                if (strncmp(optarg, "mon:", 4) == 0) {
                    default_monitor = 0;
                }
                break;
            case QEMU_OPTION_debugcon:
                add_device_config(DEV_DEBUGCON, optarg);
                break;
	    case QEMU_OPTION_loadvm:
		loadvm = optarg;
		break;
            case QEMU_OPTION_full_screen:
                full_screen = 1;
                break;
            case QEMU_OPTION_no_frame:
                no_frame = 1;
                break;
            case QEMU_OPTION_alt_grab:
                alt_grab = 1;
                break;
            case QEMU_OPTION_ctrl_grab:
                ctrl_grab = 1;
                break;
            case QEMU_OPTION_no_quit:
                no_quit = 1;
                break;
            case QEMU_OPTION_sdl:
#if defined(CONFIG_SDL) || defined(USE_ANDROID_EMU)
                display_type = DT_SDL;
#else
                fprintf(stderr, "SDL support is disabled\n");
                return 1;
#endif
                break;
            case QEMU_OPTION_pidfile:
                pid_file = optarg;
                break;
            case QEMU_OPTION_win2k_hack:
                win2k_install_hack = 1;
                break;
            case QEMU_OPTION_rtc_td_hack: {
                static GlobalProperty slew_lost_ticks[] = {
                    {
                        .driver   = "mc146818rtc",
                        .property = "lost_tick_policy",
                        .value    = "slew",
                    },
                    { /* end of list */ }
                };

                qdev_prop_register_global_list(slew_lost_ticks);
                break;
            }
            case QEMU_OPTION_acpitable:
                opts = qemu_opts_parse(qemu_find_opts("acpi"), optarg, 1);
                if (!opts) {
                    return 1;
                }
                do_acpitable_option(opts);
                break;
            case QEMU_OPTION_smbios:
                opts = qemu_opts_parse(qemu_find_opts("smbios"), optarg, 0);
                if (!opts) {
                    return 1;
                }
                do_smbios_option(opts);
                break;
            case QEMU_OPTION_enable_kvm:
                olist = qemu_find_opts("machine");
                qemu_opts_parse(olist, "accel=kvm", 0);
                break;
            case QEMU_OPTION_enable_hax:
                olist = qemu_find_opts("machine");
                qemu_opts_parse(olist, "accel=hax", 0);
                hax_disable(0);
                break;
            case QEMU_OPTION_machine:
                olist = qemu_find_opts("machine");
                opts = qemu_opts_parse(olist, optarg, 1);
                if (!opts) {
                    return 1;
                }
                optarg = qemu_opt_get(opts, "type");
                if (optarg) {
                    machine_class = machine_parse(optarg);
                    if (!machine_class) {
                        return 1;
                    }
                }
                break;
             case QEMU_OPTION_no_kvm:
                olist = qemu_find_opts("machine");
                qemu_opts_parse(olist, "accel=tcg", 0);
                break;
            case QEMU_OPTION_no_kvm_pit: {
                fprintf(stderr, "Warning: KVM PIT can no longer be disabled "
                                "separately.\n");
                break;
            }
            case QEMU_OPTION_no_kvm_pit_reinjection: {
                static GlobalProperty kvm_pit_lost_tick_policy[] = {
                    {
                        .driver   = "kvm-pit",
                        .property = "lost_tick_policy",
                        .value    = "discard",
                    },
                    { /* end of list */ }
                };

                fprintf(stderr, "Warning: option deprecated, use "
                        "lost_tick_policy property of kvm-pit instead.\n");
                qdev_prop_register_global_list(kvm_pit_lost_tick_policy);
                break;
            }
            case QEMU_OPTION_usb:
                olist = qemu_find_opts("machine");
                qemu_opts_parse(olist, "usb=on", 0);
                break;
            case QEMU_OPTION_usbdevice:
                olist = qemu_find_opts("machine");
                qemu_opts_parse(olist, "usb=on", 0);
                add_device_config(DEV_USB, optarg);
                break;
            case QEMU_OPTION_device:
                if (!qemu_opts_parse(qemu_find_opts("device"), optarg, 1)) {
                    return 1;
                }
                break;
            case QEMU_OPTION_smp:
                if (!qemu_opts_parse(qemu_find_opts("smp-opts"), optarg, 1)) {
                    return 1;
                }
                break;
	    case QEMU_OPTION_vnc:
#ifdef CONFIG_VNC
                display_remote++;
                vnc_display = optarg;
#else
                fprintf(stderr, "VNC support is disabled\n");
                return 1;
#endif
                break;
            case QEMU_OPTION_no_acpi:
                acpi_enabled = 0;
                break;
            case QEMU_OPTION_no_hpet:
                no_hpet = 1;
                break;
            case QEMU_OPTION_balloon:
                if (balloon_parse(optarg) < 0) {
                    fprintf(stderr, "Unknown -balloon argument %s\n", optarg);
                    return 1;
                }
                break;
            case QEMU_OPTION_no_reboot:
                no_reboot = 1;
                break;
            case QEMU_OPTION_no_shutdown:
                no_shutdown = 1;
                break;
            case QEMU_OPTION_show_cursor:
                cursor_hide = 0;
                break;
            case QEMU_OPTION_uuid:
                if(qemu_uuid_parse(optarg, qemu_uuid) < 0) {
                    fprintf(stderr, "Fail to parse UUID string."
                            " Wrong format.\n");
                    return 1;
                }
                qemu_uuid_set = true;
                break;
	    case QEMU_OPTION_option_rom:
		if (nb_option_roms >= MAX_OPTION_ROMS) {
		    fprintf(stderr, "Too many option ROMs\n");
            return 1;
		}
                opts = qemu_opts_parse(qemu_find_opts("option-rom"), optarg, 1);
                if (!opts) {
                    return 1;
                }
                option_rom[nb_option_roms].name = qemu_opt_get(opts, "romfile");
                option_rom[nb_option_roms].bootindex =
                    qemu_opt_get_number(opts, "bootindex", -1);
                if (!option_rom[nb_option_roms].name) {
                    fprintf(stderr, "Option ROM file is not specified\n");
                    return 1;
                }
		nb_option_roms++;
		break;
            case QEMU_OPTION_semihosting:
                semihosting_enabled = 1;
                break;
            case QEMU_OPTION_tdf:
                fprintf(stderr, "Warning: user space PIT time drift fix "
                                "is no longer supported.\n");
                break;
            case QEMU_OPTION_name:
                opts = qemu_opts_parse(qemu_find_opts("name"), optarg, 1);
                if (!opts) {
                    return 1;
                }
                break;
            case QEMU_OPTION_prom_env:
                if (nb_prom_envs >= MAX_PROM_ENVS) {
                    fprintf(stderr, "Too many prom variables\n");
                    return 1;
                }
                prom_envs[nb_prom_envs] = optarg;
                nb_prom_envs++;
                break;
            case QEMU_OPTION_old_param:
                old_param = 1;
                break;
            case QEMU_OPTION_clock:
                /* Clock options no longer exist.  Keep this option for
                 * backward compatibility.
                 */
                break;
            case QEMU_OPTION_startdate:
                if (!configure_rtc_date_offset(optarg, 1)) {
                    return 1;
                }
                break;
            case QEMU_OPTION_rtc:
                opts = qemu_opts_parse(qemu_find_opts("rtc"), optarg, 0);
                if (!opts) {
                    return 1;
                }
                if (!configure_rtc(opts)) {
                    return 1;
                }
                break;
            case QEMU_OPTION_tb_size:
                tcg_tb_size = strtol(optarg, NULL, 0);
                if (tcg_tb_size < 0) {
                    tcg_tb_size = 0;
                }
                break;
            case QEMU_OPTION_icount:
                icount_opts = qemu_opts_parse(qemu_find_opts("icount"),
                                              optarg, 1);
                if (!icount_opts) {
                    return 1;
                }
                break;
            case QEMU_OPTION_incoming:
                incoming = optarg;
                runstate_set(RUN_STATE_INMIGRATE);
                break;
            case QEMU_OPTION_nodefaults:
                has_defaults = 0;
                break;
            case QEMU_OPTION_xen_domid:
                if (!(xen_available())) {
                    printf("Option %s not supported for this target\n", popt->name);
                    return 1;
                }
                xen_domid = atoi(optarg);
                break;
            case QEMU_OPTION_xen_create:
                if (!(xen_available())) {
                    printf("Option %s not supported for this target\n", popt->name);
                    return 1;
                }
                xen_mode = XEN_CREATE;
                break;
            case QEMU_OPTION_xen_attach:
                if (!(xen_available())) {
                    printf("Option %s not supported for this target\n", popt->name);
                    return 1;
                }
                xen_mode = XEN_ATTACH;
                break;
            case QEMU_OPTION_trace:
            {
                opts = qemu_opts_parse(qemu_find_opts("trace"), optarg, 0);
                if (!opts) {
                    return 1;
                }
                trace_events = qemu_opt_get(opts, "events");
                trace_file = qemu_opt_get(opts, "file");
                break;
            }
            case QEMU_OPTION_readconfig:
                {
                    int ret = qemu_read_config_file(optarg);
                    if (ret < 0) {
                        fprintf(stderr, "read config %s: %s\n", optarg,
                            strerror(-ret));
                        return 1;
                    }
                    break;
                }
            case QEMU_OPTION_spice:
                olist = qemu_find_opts("spice");
                if (!olist) {
                    fprintf(stderr, "spice is not supported by this qemu build.\n");
                    return 1;
                }
                opts = qemu_opts_parse(olist, optarg, 0);
                if (!opts) {
                    return 1;
                }
                display_remote++;
                break;
            case QEMU_OPTION_writeconfig:
                {
                    FILE *fp;
                    if (strcmp(optarg, "-") == 0) {
                        fp = stdout;
                    } else {
                        fp = fopen(optarg, "w");
                        if (fp == NULL) {
                            fprintf(stderr, "open %s: %s\n", optarg, strerror(errno));
                            return 1;
                        }
                    }
                    qemu_config_write(fp);
                    if (fp != stdout) {
                        fclose(fp);
                    }
                    break;
                }
            case QEMU_OPTION_qtest:
                qtest_chrdev = optarg;
                break;
            case QEMU_OPTION_qtest_log:
                qtest_log = optarg;
                break;
            case QEMU_OPTION_sandbox:
                opts = qemu_opts_parse(qemu_find_opts("sandbox"), optarg, 1);
                if (!opts) {
                    return 1;
                }
                break;
            case QEMU_OPTION_add_fd:
#ifndef _WIN32
                opts = qemu_opts_parse(qemu_find_opts("add-fd"), optarg, 0);
                if (!opts) {
                    return 1;
                }
#else
                error_report("File descriptor passing is disabled on this "
                             "platform");
                return 1;
#endif
                break;
            case QEMU_OPTION_object:
                opts = qemu_opts_parse(qemu_find_opts("object"), optarg, 1);
                if (!opts) {
                    return 1;
                }
                break;
            case QEMU_OPTION_realtime:
                opts = qemu_opts_parse(qemu_find_opts("realtime"), optarg, 0);
                if (!opts) {
                    return 1;
                }
                enable_mlock = qemu_opt_get_bool(opts, "mlock", true);
                break;
            case QEMU_OPTION_msg:
                opts = qemu_opts_parse(qemu_find_opts("msg"), optarg, 0);
                if (!opts) {
                    return 1;
                }
                configure_msg(opts);
                break;
            case QEMU_OPTION_dump_vmstate:
                if (vmstate_dump_file) {
                    fprintf(stderr, "qemu: only one '-dump-vmstate' "
                            "option may be given\n");
                    return 1;
                }
                vmstate_dump_file = fopen(optarg, "w");
                if (vmstate_dump_file == NULL) {
                    fprintf(stderr, "open %s: %s\n", optarg, strerror(errno));
                    return 1;
                }
                break;
#ifdef CONFIG_ANDROID
            case QEMU_OPTION_netspeed:
                android_op_netspeed = (char*)optarg;
                break;
            case QEMU_OPTION_netdelay:
                android_op_netdelay = (char*)optarg;
                break;
            case QEMU_OPTION_netfast:
                android_op_netfast = 1;
                break;
            case QEMU_OPTION_boot_property:
                save_cmd_property((char*)optarg);
                break;
            case QEMU_OPTION_lcd_density:
                lcd_density = strtol(optarg, (char **) &optarg, 10);
                switch (lcd_density) {
                    case LCD_DENSITY_LDPI:
                    case LCD_DENSITY_MDPI:
                    case LCD_DENSITY_TVDPI:
                    case LCD_DENSITY_HDPI:
                    case LCD_DENSITY_280DPI:
                    case LCD_DENSITY_XHDPI:
                    case LCD_DENSITY_360DPI:
                    case LCD_DENSITY_400DPI:
                    case LCD_DENSITY_420DPI:
                    case LCD_DENSITY_XXHDPI:
                    case LCD_DENSITY_560DPI:
                    case LCD_DENSITY_XXXHDPI:
                        break;
                    default:
                        fprintf(stderr, "qemu: available lcd densities are: "
                                "120, 160, 213, 240, 280, 320, 360, 400, 420, 480, 560, 640\n");
                        return 1;
                }
                break;
            case QEMU_OPTION_dns_server:
                android_op_dns_server = (char*)optarg;
                break;

            case QEMU_OPTION_list_webcam:
              android_list_web_cameras();
              return 0;

#ifdef USE_ANDROID_EMU
            case QEMU_OPTION_http_proxy:
                op_http_proxy = (char*)optarg;
                break;
            case QEMU_OPTION_android_hw:
                android_hw_file = optarg;
                break;
#endif  // USE_ANDROID_EMU
#endif  // CONFIG_ANDROID
            default:
                os_parse_cmd_args(popt->index, optarg);
            }
        }
    }

    loc_set_none();

    os_daemonize();

    if (qemu_init_main_loop(&main_loop_err)) {
        error_report("%s", error_get_pretty(main_loop_err));
        return 1;
    }

#ifdef CONFIG_ANDROID

#ifdef USE_ANDROID_EMU
    /* Ensure Looper implementation for this thread is based on the QEMU
     * main loop. */
    qemu_looper_setForThread();

    /* make sure qemud is initialized before any calls to it */
    android_qemu2_qemud_init();

    boot_property_init_service();
    android_hw_control_init();

    socket_drainer_start(looper_getForThread());
    android_wear_agent_start(looper_getForThread());

    if (!android_hw_file) {
        error_report("Missing -android-hw <file> option!");
        return 1;
    }

    CIniFile* hw_ini = iniFile_newFromFile(android_hw_file);
    if (hw_ini == NULL) {
        error_report("Could not find %s file.", android_hw_file);
        return 1;
    }

    androidHwConfig_init(android_hw, 0);
    androidHwConfig_read(android_hw, hw_ini);

    /* If we're loading VM from a snapshot, make sure that the current HW config
     * matches the one with which the VM has been saved. */
    if (loadvm && *loadvm && !snaphost_match_configs(hw_ini, loadvm)) {
        error_report("HW config doesn't match the one in the snapshot");
        return 0;
    }

    iniFile_free(hw_ini);

    {
        int width  = android_hw->hw_lcd_width;
        int height = android_hw->hw_lcd_height;
        int depth  = android_hw->hw_lcd_depth;

        /* A bit of sanity checking */
        if (width <= 0 || height <= 0    ||
            (depth != 16 && depth != 32) ||
            ((width & 1) != 0)  )
        {
            error_report("Invalid display configuration (%d,%d,%d)",
                  width, height, depth);
            return 1;
        }
        android_display_width  = width;
        android_display_height = height;
        android_display_bpp    = depth;
    }

    /* Initialize camera */
    android_camera_service_init();

    /* Initialize multi-touch emulation. */
    if (androidHwConfig_isScreenMultiTouch(android_hw)) {
        mts_port_create(NULL, gQAndroidUserEventAgent, gQAndroidDisplayAgent);
    }

    /* qemu.gles will be read by the OpenGL ES emulation libraries.
     * If set to 0, the software GL ES renderer will be used as a fallback.
     * If the parameter is undefined, this means the system image runs
     * inside an emulator that doesn't support GPU emulation at all.
     *
     * The GL ES renderer cannot start properly if GPU emulation is disabled
     * because this requires changing the LD_LIBRARY_PATH before launching
     * the emulation engine. */
    int qemu_gles = 0;
    is_opengl_alive = 1;
    if (android_hw->hw_gpu_enabled) {
        if (strcmp(android_hw->hw_gpu_mode, "guest") != 0) {
            if (android_initOpenglesEmulation() != 0 ||
                android_startOpenglesRenderer(android_hw->hw_lcd_width,
                                              android_hw->hw_lcd_height) != 0) {
                is_opengl_alive = 0;
            } else {
                android_display_use_host_gpu = 1;
                qemu_gles = 1;   // Using emugl
            }
        } else {
            qemu_gles = 2;   // Using guest
        }
    }
    if (qemu_gles) {
        char  tmp[64];
        snprintf(tmp, sizeof(tmp), "%d", 0x20000);
        boot_property_add("ro.opengles.version", tmp);
    }

    /* Set the VM's max heap size, passed as a boot property */
    if (android_hw->vm_heapSize > 0) {
        char  temp[64];
        snprintf(temp, sizeof(temp), "%dm", android_hw->vm_heapSize);
        boot_property_add("dalvik.vm.heapsize",temp);
    }

    /* From API 19 and above, the platform provides an explicit property for low memory devices. */
    if (android_hw->hw_ramSize <= 512) {
        boot_property_add("ro.config.low_ram", "true");
    }

    /* Initialize presence of hardware nav button */
    boot_property_add("qemu.hw.mainkeys", android_hw->hw_mainKeys ? "1" : "0");

    if (android_hw->hw_gsmModem) {
        if (android_qemud_get_channel(ANDROID_QEMUD_GSM,
                                      &android_modem_serial_line) < 0) {
            error_report("could not initialize qemud 'gsm' channel");
            return 1;
        }
    }

    if (android_hw->hw_gps) {
        if (android_qemud_get_channel(ANDROID_QEMUD_GPS,
                                      &android_gps_serial_line) < 0) {
            error_report("could not initialize qemud 'gps' channel");
            return 1;
        }
    }

    if (lcd_density) {
        char temp[8];
        snprintf(temp, sizeof(temp), "%d", lcd_density);
        boot_property_add("qemu.sf.lcd_density", temp);
    }

    /* Initialize net speed and delays stuff. */
    if (android_parse_network_speed(android_op_netspeed) < 0) {
        fprintf(stderr, "invalid -netspeed parameter '%s'",
                android_op_netspeed);
        return 1;
    }

    if (android_parse_network_latency(android_op_netdelay) < 0) {
        fprintf(stderr, "invalid -netdelay parameter '%s'",
                android_op_netdelay);
        return 1;
    }

    if (android_op_netfast) {
        qemu_net_download_speed = 0;
        qemu_net_upload_speed = 0;
        qemu_net_min_latency = 0;
        qemu_net_max_latency = 0;
    }

    int dns_count = 0;
    if (android_op_dns_server) {
        dns_count = slirp_parse_dns_servers(android_op_dns_server);
        if (dns_count == -2) {
            // Special case for better user feedback on this error message
            fprintf(stderr,
                    "too many servers specified in -dns-server-parameter "
                    "argument '%s'. A maximum of %d is supported.\n",
                    android_op_dns_server,
                    slirp_get_max_dns_servers());
            return 1;
        } else if (dns_count < 0) {
            fprintf(stderr, "invalid -dns-server parameter '%s'\n",
                    android_op_dns_server);
            return 1;
        }
        if (dns_count == 0) {
            printf("### WARNING: will use system default DNS server\n");
        }
    }
    if (dns_count == 0) {
        dns_count = slirp_get_system_dns_servers();
        if (dns_count < 0) {
            printf("### WARNING: unable to configure any DNS servers, "
                   "name resolution will not work\n");
        }
    }
    if (dns_count > 1) {
        additional_kernel_params = g_strdup_printf("ndns=%d", dns_count);
    }

#endif // USE_ANDROID_EMU

#endif // CONFIG_ANDROID

    if (qemu_opts_foreach(qemu_find_opts("sandbox"), parse_sandbox, NULL, 0)) {
        return 1;
    }

    if (qemu_opts_foreach(qemu_find_opts("name"), parse_name, NULL, 1)) {
        return 1;
    }

#ifndef _WIN32
    if (qemu_opts_foreach(qemu_find_opts("add-fd"), parse_add_fd, NULL, 1)) {
        return 1;
    }

    if (qemu_opts_foreach(qemu_find_opts("add-fd"), cleanup_add_fd, NULL, 1)) {
        return 1;
    }
#endif

    if (machine_class == NULL) {
        fprintf(stderr, "No machine specified, and there is no default.\n"
                "Use -machine help to list supported machines!\n");
        return 1;
    }

    current_machine = MACHINE(object_new(object_class_get_name(
                          OBJECT_CLASS(machine_class))));
    object_property_add_child(object_get_root(), "machine",
                              OBJECT(current_machine), &error_abort);
    cpu_exec_init_all();

    if (machine_class->hw_version) {
        qemu_set_version(machine_class->hw_version);
    }

    /* Init CPU def lists, based on config
     * - Must be called after all the qemu_read_config_file() calls
     * - Must be called before list_cpus()
     * - Must be called before machine->init()
     */
    cpudef_init();

    if (cpu_model && is_help_option(cpu_model)) {
        list_cpus(stdout, &fprintf, cpu_model);
        return 0;
    }

    /* Open the logfile at this point, if necessary. We can't open the logfile
     * when encountering either of the logging options (-d or -D) because the
     * other one may be encountered later on the command line, changing the
     * location or level of logging.
     */
    if (log_mask) {
        int mask;
        if (log_file) {
            qemu_set_log_filename(log_file);
        }

        mask = qemu_str_to_log_mask(log_mask);
        if (!mask) {
            qemu_print_log_usage(stdout);
            return 1;
        }
        qemu_set_log(mask);
    }

    if (!is_daemonized()) {
        if (!trace_init_backends(trace_events, trace_file)) {
            return 1;
        }
    }

    /* If no data_dir is specified then try to find it relative to the
       executable path.  */
    if (data_dir_idx < ARRAY_SIZE(data_dir)) {
        data_dir[data_dir_idx] = os_find_datadir();
        if (data_dir[data_dir_idx] != NULL) {
            data_dir_idx++;
        }
    }
    /* If all else fails use the install path specified when building. */
    if (data_dir_idx < ARRAY_SIZE(data_dir)) {
        data_dir[data_dir_idx++] = CONFIG_QEMU_DATADIR;
    }

    if (!smp_parse(qemu_opts_find(qemu_find_opts("smp-opts"), NULL))) {
        return false;
    }

    machine_class->max_cpus = machine_class->max_cpus ?: 1; /* Default to UP */
    if (smp_cpus > machine_class->max_cpus) {
        fprintf(stderr, "Number of SMP cpus requested (%d), exceeds max cpus "
                "supported by machine `%s' (%d)\n", smp_cpus,
                machine_class->name, machine_class->max_cpus);
        return 1;
    }

    /*
     * Get the default machine options from the machine if it is not already
     * specified either by the configuration file or by the command line.
     */
    if (machine_class->default_machine_opts) {
        qemu_opts_set_defaults(qemu_find_opts("machine"),
                               machine_class->default_machine_opts, 0);
    }

    qemu_opts_foreach(qemu_find_opts("device"), default_driver_check, NULL, 0);
    qemu_opts_foreach(qemu_find_opts("global"), default_driver_check, NULL, 0);

    if (!vga_model && !default_vga) {
        vga_interface_type = VGA_DEVICE;
    }
    if (!has_defaults || machine_class->no_serial) {
        default_serial = 0;
    }
    if (!has_defaults || machine_class->no_parallel) {
        default_parallel = 0;
    }
    if (!has_defaults || !machine_class->use_virtcon) {
        default_virtcon = 0;
    }
    if (!has_defaults || !machine_class->use_sclp) {
        default_sclp = 0;
    }
    if (!has_defaults || machine_class->no_floppy) {
        default_floppy = 0;
    }
    if (!has_defaults || machine_class->no_cdrom) {
        default_cdrom = 0;
    }
    if (!has_defaults || machine_class->no_sdcard) {
        default_sdcard = 0;
    }
    if (!has_defaults) {
        default_monitor = 0;
        default_net = 0;
        default_vga = 0;
    }

    if (is_daemonized()) {
        /* According to documentation and historically, -nographic redirects
         * serial port, parallel port and monitor to stdio, which does not work
         * with -daemonize.  We can redirect these to null instead, but since
         * -nographic is legacy, let's just error out.
         * We disallow -nographic only if all other ports are not redirected
         * explicitly, to not break existing legacy setups which uses
         * -nographic _and_ redirects all ports explicitly - this is valid
         * usage, -nographic is just a no-op in this case.
         */
        if (display_type == DT_NOGRAPHIC
            && (default_parallel || default_serial
                || default_monitor || default_virtcon)) {
            fprintf(stderr, "-nographic can not be used with -daemonize\n");
            return 1;
        }
#ifdef CONFIG_CURSES
        if (display_type == DT_CURSES) {
            fprintf(stderr, "curses display can not be used with -daemonize\n");
            return 1;
        }
#endif
    }

    if (display_type == DT_NOGRAPHIC) {
        if (default_parallel)
            add_device_config(DEV_PARALLEL, "null");
        if (default_serial && default_monitor) {
            add_device_config(DEV_SERIAL, "mon:stdio");
        } else if (default_virtcon && default_monitor) {
            add_device_config(DEV_VIRTCON, "mon:stdio");
        } else if (default_sclp && default_monitor) {
            add_device_config(DEV_SCLP, "mon:stdio");
        } else {
            if (default_serial)
                add_device_config(DEV_SERIAL, "stdio");
            if (default_virtcon)
                add_device_config(DEV_VIRTCON, "stdio");
            if (default_sclp) {
                add_device_config(DEV_SCLP, "stdio");
            }
            if (default_monitor) {
                if (!monitor_parse("stdio", "readline")) {
                    return 1;
                }
            }
        }
    } else {
        if (default_serial)
            add_device_config(DEV_SERIAL, "vc:80Cx24C");
        if (default_parallel)
            add_device_config(DEV_PARALLEL, "vc:80Cx24C");
        if (default_monitor) {
            if (!monitor_parse("vc:80Cx24C", "readline")) {
                return 1;
            }
        }
        if (default_virtcon)
            add_device_config(DEV_VIRTCON, "vc:80Cx24C");
        if (default_sclp) {
            add_device_config(DEV_SCLP, "vc:80Cx24C");
        }
    }

    if (display_type == DT_DEFAULT && !display_remote) {
#if defined(CONFIG_GTK)
        display_type = DT_GTK;
#elif defined(CONFIG_SDL) || defined(CONFIG_COCOA) || defined(USE_ANDROID_EMU)
        display_type = DT_SDL;
#elif defined(CONFIG_VNC)
        vnc_display = "localhost:0,to=99";
        show_vnc_port = 1;
#else
        display_type = DT_NONE;
#endif
    }

    if ((no_frame || alt_grab || ctrl_grab) && display_type != DT_SDL) {
        fprintf(stderr, "-no-frame, -alt-grab and -ctrl-grab are only valid "
                        "for SDL, ignoring option\n");
    }
    if (no_quit && (display_type != DT_GTK && display_type != DT_SDL)) {
        fprintf(stderr, "-no-quit is only valid for GTK and SDL, "
                        "ignoring option\n");
    }

#if defined(CONFIG_GTK)
    if (display_type == DT_GTK) {
        early_gtk_display_init();
    }
#endif

#ifndef USE_ANDROID_EMU
    // When using AndroidEmu, this "main" is no longer the entry point on the
    // main thread. It is in fact called on a secondary thread, and socket
    // initialization is long finished (See android-qemu2-glue/main.cpp).
    socket_init();
#endif

    if (qemu_opts_foreach(qemu_find_opts("chardev"), chardev_init_func, NULL, 1) != 0)
        return 1;
#ifdef CONFIG_VIRTFS
    if (qemu_opts_foreach(qemu_find_opts("fsdev"), fsdev_init_func, NULL, 1) != 0) {
        return 1;
    }
#endif

    if (pid_file && qemu_create_pidfile(pid_file) != 0) {
        fprintf(stderr, "Could not acquire pid file: %s\n", strerror(errno));
        return 1;
    }
#ifdef USE_ANDROID_EMU
    uint64_t hax_max_ram = 0;
    if (hax_get_max_ram(&hax_max_ram) == 0 && hax_max_ram > 0) {
        char str[32] = {0};
        snprintf(str, sizeof(str) - 1, "%"PRIu64, hax_max_ram);
        crashhandler_add_string("hax_max_ram.txt", str);
        if (ram_size > hax_max_ram) {
            const int requested_meg = ram_size / (1024 * 1024);
            const int actual_meg = hax_max_ram / (1024 * 1024);
            fprintf(stderr, "Warning: requested ram_size %dM too big, reduced to %dM\n",
                    requested_meg, actual_meg);
            ram_size = hax_max_ram;
        }
    }
#endif

    /* store value for the future use */
    qemu_opt_set_number(qemu_find_opts_singleton("memory"), "size", ram_size);
    hax_pre_init(ram_size);

    if (qemu_opts_foreach(qemu_find_opts("device"), device_help_func, NULL, 0)
        != 0) {
        return 0;
    }

    if (qemu_opts_foreach(qemu_find_opts("object"),
                          object_create, NULL, 0) != 0) {
        return 1;
    }

    machine_opts = qemu_get_machine_opts();
    if (qemu_opt_foreach(machine_opts, machine_set_property, current_machine,
                         1) < 0) {
        object_unref(OBJECT(current_machine));
        return 1;
    }

    if (configure_accelerator(current_machine)) {
        return 1;
    }


    if (qtest_chrdev) {
        Error *local_err = NULL;
        qtest_init(qtest_chrdev, qtest_log, &local_err);
        if (local_err) {
            error_report("%s", error_get_pretty(local_err));
            error_free(local_err);
            return 1;
        }
    }

    machine_opts = qemu_get_machine_opts();
    kernel_filename = qemu_opt_get(machine_opts, "kernel");
    initrd_filename = qemu_opt_get(machine_opts, "initrd");
    kernel_cmdline = qemu_opt_get(machine_opts, "append");
    bios_name = qemu_opt_get(machine_opts, "firmware");

    boot_order = machine_class->default_boot_order;
    opts = qemu_opts_find(qemu_find_opts("boot-opts"), NULL);
    if (opts) {
        char *normal_boot_order;
        const char *order, *once;

        order = qemu_opt_get(opts, "order");
        if (order) {
            if (!validate_bootdevices(order)) {
                return 1;
            }
            boot_order = order;
        }

        once = qemu_opt_get(opts, "once");
        if (once) {
            if (!validate_bootdevices(once)) {
                return 1;
            }
            normal_boot_order = g_strdup(boot_order);
            boot_order = once;
            qemu_register_reset(restore_boot_order, normal_boot_order);
        }

        boot_menu = qemu_opt_get_bool(opts, "menu", boot_menu);
        boot_strict = qemu_opt_get_bool(opts, "strict", false);
    }

    current_machine->kernel_cmdline = kernel_cmdline ? (char *)kernel_cmdline : "";

#ifdef CONFIG_ANDROID
    if (additional_kernel_params) {
        char* combined = g_strdup_printf("%s %s",
                                         current_machine->kernel_cmdline,
                                         additional_kernel_params);
        current_machine->kernel_cmdline = combined;
        // Free the original buffer and put the newly allocated one in there
        // to make sure it gets deallocated.
        g_free(additional_kernel_params);
        additional_kernel_params = combined;
    }
#endif  // CONFIG_ANDROID

    linux_boot = (kernel_filename != NULL);

    if (!linux_boot && *kernel_cmdline != '\0') {
        fprintf(stderr, "-append only allowed with -kernel option\n");
        return 1;
    }

    if (!linux_boot && initrd_filename != NULL) {
        fprintf(stderr, "-initrd only allowed with -kernel option\n");
        return 1;
    }

    if (!linux_boot && qemu_opt_get(machine_opts, "dtb")) {
        fprintf(stderr, "-dtb only allowed with -kernel option\n");
        return 1;
    }

    os_set_line_buffering();

    qemu_init_cpu_loop();
    qemu_mutex_lock_iothread();

#ifdef CONFIG_SPICE
    /* spice needs the timers to be initialized by this point */
    qemu_spice_init();
#endif

    cpu_ticks_init();
    if (icount_opts) {
        if (kvm_enabled() || xen_enabled() || hax_enabled()) {
            fprintf(stderr, "-icount is not allowed with kvm or xen or hax\n");
            return 1;
        }
        configure_icount(icount_opts, &error_abort);
        qemu_opts_del(icount_opts);
    }

    /* clean up network at qemu process termination */
    atexit(&net_cleanup);

    if (net_init_clients() < 0) {
        return 1;
    }

#ifdef CONFIG_TPM
    if (tpm_init() < 0) {
        return 1;
    }
#endif

    /* init the bluetooth world */
    if (foreach_device_config(DEV_BT, bt_parse))
        return 1;

    if (!xen_enabled()) {
        /* On 32-bit hosts, QEMU is limited by virtual address space */
        if (ram_size > (2047 << 20) && HOST_LONG_BITS == 32) {
            fprintf(stderr, "qemu: at most 2047 MB RAM can be simulated\n");
            return 1;
        }
    }

    blk_mig_init();
    ram_mig_init();

    /* If the currently selected machine wishes to override the units-per-bus
     * property of its default HBA interface type, do so now. */
    if (machine_class->units_per_default_bus) {
        override_max_devs(machine_class->block_default_type,
                          machine_class->units_per_default_bus);
    }

    /* open the virtual block devices */
    if (snapshot)
        qemu_opts_foreach(qemu_find_opts("drive"), drive_enable_snapshot, NULL, 0);
    if (qemu_opts_foreach(qemu_find_opts("drive"), drive_init_func,
                          &machine_class->block_default_type, 1) != 0) {
        return 1;
    }

    if (!default_drive(default_cdrom, snapshot,
                       machine_class->block_default_type, 2, CDROM_OPTS)) {
        return 1;
    }
    if (!default_drive(default_floppy, snapshot, IF_FLOPPY, 0, FD_OPTS)) {
        return 1;
    }
    if (!default_drive(default_sdcard, snapshot, IF_SD, 0, SD_OPTS)) {
        return 1;
    }

    if (qemu_opts_foreach(qemu_find_opts("numa"), numa_init_func,
                          NULL, 1) != 0) {
        return 1;
    }

    set_numa_nodes();

    if (foreach_device_config(DEV_SERIAL, serial_parse) < 0)
        return 1;
    if (foreach_device_config(DEV_PARALLEL, parallel_parse) < 0)
        return 1;
    if (foreach_device_config(DEV_VIRTCON, virtcon_parse) < 0)
        return 1;
    if (foreach_device_config(DEV_SCLP, sclp_parse) < 0) {
        return 1;
    }
    if (foreach_device_config(DEV_DEBUGCON, debugcon_parse) < 0)
        return 1;

    /* If no default VGA is requested, the default is "none".  */
    if (default_vga) {
        if (machine_class->default_display) {
            vga_model = machine_class->default_display;
        } else if (cirrus_vga_available()) {
            vga_model = "cirrus";
        } else if (vga_available()) {
            vga_model = "std";
        }
    }
    if (vga_model) {
        select_vgahw(vga_model);
    }

    if (watchdog) {
        i = select_watchdog(watchdog);
        if (i > 0)
            return (i == 1 ? 1 : 0);
    }

    if (machine_class->compat_props) {
        qdev_prop_register_global_list(machine_class->compat_props);
    }
    qemu_add_globals();

    qdev_machine_init();

    current_machine->ram_size = ram_size;
    current_machine->maxram_size = maxram_size;
    current_machine->ram_slots = ram_slots;
    current_machine->boot_order = boot_order;
    current_machine->cpu_model = cpu_model;

    machine_class->init(current_machine);
#ifdef USE_ANDROID_EMU
    if (android_init_error_occurred()) {
        // Something went wrong when initializing the virtual machine
        return 1;
    }
#endif

    if (!realtime_init()) {
        return 1;
    }

    if (!audio_init()) {
        return 1;
    }

    cpu_synchronize_all_post_init();

    set_numa_modes();

    if (hax_enabled()) {
        if (hax_sync_vcpus() < 0) {
            fprintf(stderr, "Internal error: initial hax sync failed\n");
            return 1;
        }
    }

    /* init USB devices */
    if (usb_enabled(false)) {
        if (foreach_device_config(DEV_USB, usb_parse) < 0)
            return 1;
    }

    /* init generic devices */
    if (qemu_opts_foreach(qemu_find_opts("device"), device_init_func, NULL, 1) != 0)
        return 1;

    /* Did we create any drives that we failed to create a device for? */
    drive_check_orphaned();

    net_check_clients();

    ds = init_displaystate();

    /* init local displays */
    switch (display_type) {
    case DT_NOGRAPHIC:
        (void)ds;	/* avoid warning if no display is configured */
        break;
#if defined(CONFIG_CURSES)
    case DT_CURSES:
        curses_display_init(ds, full_screen);
        break;
#endif
#if defined(CONFIG_SDL) || defined(USE_ANDROID_EMU)
    case DT_SDL:
        if (!sdl_display_init(ds, full_screen, no_frame)) {
            return 1;
        }
        break;
#elif defined(CONFIG_COCOA)
    case DT_SDL:
        cocoa_display_init(ds, full_screen);
        break;
#endif
#if defined(CONFIG_GTK)
    case DT_GTK:
        gtk_display_init(ds, full_screen, grab_on_hover);
        break;
#endif
    default:
        break;
    }

    /* must be after terminal init, SDL library changes signal handlers */
    os_setup_signal_handling();

#ifdef CONFIG_VNC
    /* init remote displays */
    if (vnc_display) {
        Error *local_err = NULL;
        vnc_display_init(ds);
        vnc_display_open(ds, vnc_display, &local_err);
        if (local_err != NULL) {
            error_report("Failed to start VNC server on `%s': %s",
                         vnc_display, error_get_pretty(local_err));
            error_free(local_err);
            return 1;
        }

        if (show_vnc_port) {
            printf("VNC server running on `%s'\n", vnc_display_local_addr(ds));
        }
    }
#endif
#ifdef CONFIG_SPICE
    if (using_spice) {
        qemu_spice_display_init();
    }
#endif

    if (foreach_device_config(DEV_GDB, gdbserver_start) < 0) {
        return 1;
    }

    qdev_machine_creation_done();

    if (rom_load_all() != 0) {
        fprintf(stderr, "rom loading failed\n");
        return 1;
    }

#if defined(USE_ANDROID_EMU)
    /* call android-specific setup function */
    qemu_android_emulation_setup();

    extern void android_emulator_set_base_port(int);
    android_emulator_set_base_port(android_base_port);
#endif

    if (qemu_opts_foreach(qemu_find_opts("mon"), mon_init_func, NULL, 1) != 0) {
        return 1;
    }

#ifdef CONFIG_ANDROID
    // Parse the System boot parameters from the command line last,
    // so they take precedence
    process_cmd_properties();
#endif  // CONFIG_ANDROID

    /* TODO: once all bus devices are qdevified, this should be done
     * when bus is created by qdev.c */
    qemu_register_reset(qbus_reset_all_fn, sysbus_get_default());
    qemu_run_machine_init_done_notifiers();

    /* Done notifiers can load ROMs */
    rom_load_done();

    qemu_system_reset(VMRESET_SILENT);
    if (loadvm) {
        if (load_vmstate(loadvm) < 0) {
            autostart = 0;
        }
    }

    qdev_prop_check_globals();
    if (vmstate_dump_file) {
        /* dump and return */
        dump_vmstate_json_to_file(vmstate_dump_file);
        return 0;
    }

    if (incoming) {
        Error *local_err = NULL;
        qemu_start_incoming_migration(incoming, &local_err);
        if (local_err) {
            error_report("-incoming %s: %s", incoming,
                         error_get_pretty(local_err));
            error_free(local_err);
            return 1;
        }
    } else if (autostart) {
        vm_start();
    }

    os_setup_post();

    if (is_daemonized()) {
        if (!trace_init_backends(trace_events, trace_file)) {
            return 1;
        }
    }

    // Initialize reporting right before entering main loop.
    // We want to track performance of a running emulator, ignoring any early
    // exits as a result of incorrect setup.
    if (!android_reporting_setup()) {
        return 1;
    }

    main_loop();
#ifdef USE_ANDROID_EMU
    crashhandler_exitmode("after main_loop");
#endif
    bdrv_close_all();
    pause_all_vcpus();
    res_free();
#ifdef CONFIG_TPM
    tpm_cleanup();
#endif

    android_reporting_teardown();

    return 0;
}
