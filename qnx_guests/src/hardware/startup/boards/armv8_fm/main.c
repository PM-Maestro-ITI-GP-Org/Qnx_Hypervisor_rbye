/*
 * $QNXLicenseC:
 * Copyright 2017, QNX Software Systems. All Rights Reserved.
 *
 * You must obtain a written license from and pay applicable license fees to QNX
 * Software Systems before you may reproduce, modify or distribute this software,
 * or any work that includes all or part of this software.   Free development
 * licenses are available for evaluation and non-commercial purposes.  For more
 * information visit http://licensing.qnx.com or email licensing@qnx.com.
 *
 * This file may contain contributions from others.  Please review this entire
 * file for other proprietary rights or license notices, as well as the QNX
 * Development Suite License Guide at http://licensing.qnx.com/license-guide/
 * for other information.
 * $
 */

/*
 * ARMv8 Foundation Model
 */

#include <startup.h>
#include <aarch64/psci.h>
#include "armv8_fm_startup.h"

#define DEFAULT_CPU_FREQ 100000000

#if defined(__aarch64__)
    #define FDT_REG     0
#else
    #define FDT_REG     2
#endif


/**
 * Adjust bootstrap executable command lines based on "/chosen" node's "bootargs" property
 * @param   bap         argument and environment list
 * @param   name    string associated to list
 */
void
tweak_cmdline(struct bootargs_entry *bap, const char *name) {
    if(boot_regs[FDT_REG]) {
        fdt_tweak_cmdline(bap, name, boot_regs[FDT_REG]);
    }
}


/*
 * main()
 *  Startup program executing out of RAM
 *
 * 1. It gathers information about the system and places it in a structure
 *    called the system page. The kernel references this structure to
 *    determine everything it needs to know about the system. This structure
 *    is also available to user programs (read only if protection is on)
 *    via _syspage->.
 *
 * 2. It (optionally) turns on the MMU and starts the next program
 *    in the image file system.
 */
int
main(int argc, char **argv, char **envv)
{
    // Define a reboot callout for the case where there is no FDT, or
    // it could not be determined from FDT what reboot method is required
    extern struct callout_rtn reboot;
    static const struct callout_slot callouts_fallback[] = {
        {
	    .offset = offsetof(struct callout_entry, reboot),
	    .callout = &reboot
	},
    };

    static const struct debug_device debug_devices[] = {
        {
            .name = "pl011",
	    .defaults = {[0] = "0x1c090000^0.38400.24000000",
                         [1] = "0x1c0A0000^0.38400.24000000"
                        },
            .init = init_pl011,
            .put = put_pl011,
            .callouts = {
                [DEBUG_DISPLAY_CHAR] = &display_char_pl011,
                [DEBUG_POLL_KEY] = &poll_key_pl011,
                [DEBUG_BREAK_DETECT] = &break_detect_pl011,
            }
        },
    };

    int         opt;

    // Check for and initialize flattened device tree
    if(boot_regs[FDT_REG]) {
        fdt_init(boot_regs[FDT_REG]);
        fdt_psci_configure();
    }

    /*
     * FIXME_AARCH64: need to figure out how to get RAM size
     */
    paddr_t     memsize = MEG(1024);

    // Allow to specify specific GIC version
    unsigned gic_version = 0;

	int use_mm = 0;
    while ((opt = getopt(argc, argv, COMMON_OPTIONS_STRING "m:23b")) != -1) {
        switch(opt) {
        case 'm':
            memsize = getsize(optarg, NULL);
            break;

        case '2':
            gic_version = 2;
            break;

        case '3':
            gic_version = 3;
            break;

        case 'b':
            use_mm = 1;
            break;

        default:
            handle_common_option(opt);
            break;
        }
    }

    /*
     * Initialise debugging output
     */
    select_debug(debug_devices, sizeof(debug_devices));

    /*
     * Setup callouts
     */

    // The choice of which reboot callout to make is likely made in
    // fdt_psci_configure(), above, result being that psci_call is set
    // accordingly. If it isn't, revert to the generic reboot callout.
    if (psci_call == &psci_hvc) {
        add_callout(offsetof(struct callout_entry, reboot), &reboot_psci_hvc);
    } else if (psci_call == &psci_smc) {
        add_callout(offsetof(struct callout_entry, reboot), &reboot_psci_smc);
    } else {
        add_callout_array(callouts_fallback, sizeof(callouts_fallback));
    }

    /*
     * Set the default cpu frequency of 100MHz unless override by -f option or if provided by FDT
     */
    if (cpu_freq == 0) {
        if (fdt_size != 0) {
            cpu_freq = fdt_get_cpu_freq();
        }

        if ( cpu_freq == 0 ) {
            cpu_freq = DEFAULT_CPU_FREQ;
        }
    }

#if defined(__aarch64__)
    if(timer_freq == 0) {
        timer_freq = aa64_sr_rd32(cntfrq_el0);
        if(timer_freq == 0) {
            timer_freq = DEFAULT_CPU_FREQ;
        }
    }
#endif

    /*
     * Collect information on all free RAM in the system
     */
    if(fdt_size != 0) {
        init_raminfo_fdt();
        fdt_asinfo();
    } else {
        add_ram(0x80000000, memsize);
    }

    /*
     * Remove RAM used by modules in the image
     */
    alloc_ram(shdr->ram_paddr, shdr->ram_size, 1);

    /*
     * Enable Hypervisor if requested (and possible)
     *
     * IMPORTANT: Refer to the aarch64 hypervisor_init comment block. The
     * placement of this call is critical because it may switch the CPU to EL2&0
     * for VHE (virtualization host extensions.)
     */
    hypervisor_init(0);

    /*
     * Initialise SMP
     */
    init_smp();

    if (shdr->flags1 & STARTUP_HDR_FLAGS1_VIRTUAL) {
        init_mmu();
    }
    init_intrinfo_fm(gic_version, use_mm);
    init_qtime();
    init_cacheattr();
    init_cpuinfo();
    init_hwinfo();
    add_typed_string(_CS_MACHINE, "ARMv8 Foundation Model");

    /*
     * Load bootstrap executables in the image file system and Initialise
     * various syspage pointers. This must be the _last_ initialisation done
     * before transferring control to the next program.
     */
    init_system_private();

    /*
     * This is handy for debugging a new version of the startup program.
     * Commenting this line out will save a great deal of code.
     */
    print_syspage();

    return 0;
}
