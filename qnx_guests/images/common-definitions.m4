## Boot parameters
define(`__LD_QNX__',            `ldqnx-64.so.2')
define(`__BOOT_ADDR__',         `0x80000000')
define(`__ARCH__',              `aarch64le')
define(`__TYPE__',              `elf')
define(`__COMPRESS_ATTR__',     `')
define(`__PROCNTO_MODULES__',   `')
define(`__STARTUP__',           `startup-armv8_fm')
define(`__STARTUP_OPTS__',      `-H')
define(`__PROCNTO__',           `procnto-smp-instr')
define(`__PROCNTO_OPTS__',      `-v')

## Console (assume devc-virtio is being used)
define(`__CONSOLE__',           `/dev/vcon1')

## System Memory Management Unit (smmu)
ifdef(`__SMMU__', `
define(`__SMMU_START__', `
#-------------------------------------------------------------------------------
# Start smmuman if your application requires it.
# NOTE: requires that you include vdev-smmu.so in your guest qvm configuration
#
# Here we show starting it without any configuration. You could add configuration
# to the command-line, or you can configure it with the smmuman client API.
#-------------------------------------------------------------------------------
        # display_msg "Starting guest smmuman"
        # smmuman
')
')

## PCI driver
define(`__PCI_HW_DRVR__', `pci_hw-fdt.so')
define(`__PCI_HW_MODULE__', `pci_hw-fdt.so')
define(`__PCI_START__', `
#-------------------------------------------------------------------------------
# If you have PCI devices configured, and drivers that start below that
# depend on those devices being found under /dev/pci, un-comment the
# lines below. We leave it commented-out here because the standard guest
# do not have PCI devices, no FDT info for them included.
    #display_msg "Starting PCI server..."
    #pci-server --bus-scan-limit=0
    #waitfor /dev/pci
')

## Serial driver
define(`__DEVC_DRVR__', `devc-virtio, devc-serpl011')
define(`__DEVC_START__', `
#   NOTE: If using devc-virtio, it is not necessary to include another serial
#         driver, though the startup mini-driver may use this hardware.
#   # Initialise the console
#    devc-serpl011 -e -F  0x1C090000,37
#    reopen /dev/ser1

    # Here we use virtio-console as default console
    # Initialise the console
    devc-virtio 0x20000000,42
')

## Network driver
define(`__NET_DRVR__', `devs-vtnet_mmio.so')
define(`__NET_OPTS__', `-m fdt -d vtnet_mmio')
define(`__NET_DEV__', `vtnet')
define(`__NET_START__', `
__NET_COMMON_START__

    # Assign a static IP address for the guest-to-guest device, for this example, the convention is:
    #  guest 1 address: 192.168.10.1/24
    #  subsequent guests use the next address available e.g.: 192.168.10.2/24
    ifconfig vtnet1 __G2G_IP_ADDR__/24

')

## Block driver
define(`__BLOCK_DRVR__', `devb-ram, devb-virtio')
define(`__DEVB_RAM_DRVR__', `devb-ram')
#define(`__DEVB_RAM_OPTS__', `')
#define(`__DEVB_RAM_DEV__', `')

## WDT kick
define(`__WDT_DRVR__', `wdtkick')

## Customize script
define(`__CUSTOMIZE_SCRIPT_NAME__', `/scripts/board_startup.sh')
#define(`__CUSTOMIZE_SCRIPT_START__', `')
#define(`__CUSTOMIZE_SCRIPT_FILES__', `')

## Board specific files
define(`__BOARD_EARLY_START__', `')

define(`__BOARD_LATE_START__', `
#-------------------------------------------------------------------------------
    display_msg "Virtual networking comes up by default if virtio-net devices are configured for this guest"
    display_msg ""
    display_msg "A virtual block device can be started by running /scripts/block-start.sh"
    display_msg "   (this requires vdev-virtio-blk is configured for this guest)"
    display_msg "   (Note that it assumes you are using a blank RAM disk as host device)"
    display_msg ""
    display_msg "Virtual shared memory device demo driver can be started by running /scripts/shmem-start.sh"
    display_msg "   (this requires vdev-shmem is configured in this guest)"
    display_msg ""
    display_msg "Virtual watchdog device can be started and stopped by running /scripts/watchdog-start.sh and watchdog-stop.sh"
    display_msg "   (this requires a vdev-wdt-* is configured in this guest)"
    display_msg ""
    display_msg "Note: the scripts above assume that you have located your virtual devices at particular loc/intr values"
    display_msg "      that match the values passed to the corresponding driver"
    display_msg ""
')

# Because of all of the quote characters in here, temporarily change the quote character, restore after this define
define(`__BOARD_FILES__', changequote(`[[[', `]]]')[[[
# Needed by pci_hw-fdt.so
libfdt.so

#Shared memory virtual device demonstration utility
shmem-guest

# General utilities for manipulating disk partitions and filesystems
/sbin/fdisk=fdisk
/sbin/mkqnx6fs=mkqnx6fs
/sbin/mkdosfs=mkdosfs

[perms=0755] /scripts/shmem-start.sh = {
#!/bin/sh

#NOTE: the virtual device must make the location (loc) and interrupt (intr) option match the values here
shmem-guest 0x1c050000 38

}

[perms=0755] /scripts/ramdisk-init.sh = {
#!/bin/sh

if [ ! -e /dev/hd0t178 ]; then
        fdisk /dev/hd0 add -t178
        mount -e /dev/hd0
        waitfor /dev/hd0t178
        mkqnx6fs -q /dev/hd0t178
fi

mount -tqnx6 -o sync=none /dev/hd0t178 /ramdisk

}
[perms=0755] /scripts/block-start.sh = {
#!/bin/sh

        echo "Starting virtio block..."

        # Note for arm guests, we're using MMIO mode for vdev's (instead of PCI, though PCI for arm will come)
        # Values for smem and irq must match those provided in the vdev configuration (or suitably translated if needed)
        devb-virtio virtio smem=0x1c0d0000,irq=41

        waitfor /dev/hd0
        /scripts/ramdisk-init.sh  &

}

[perms=0755] /scripts/watchdog-start.sh = {
#!/bin/sh

#######################################################################
## WatchDog utility
## If guest is configured with a vdev-wdt-* device then the 'wdtkick'
## can be used to enable it, as well as give it the required regular
## kick.
#######################################################################

# Here, we assume the vdev has been configured with a 'loc' option set to
# so that it is found at the base addr of 0x1C0F0000.
# Writing value of 3 (0'b11) to offset 8 enables the watchdog
# The value written to offset 0 determines the timeout

# Kick every second & timeout set to 3 seconds (actually 6 seconds since the timer counts down twice & asserts the reset on the second expiry)
wdtkick -v -a 0x1C0F0000 -t 1000 -E 8:3 -W 0:0x47868C0
}

[perms=0755] /scripts/watchdog-stop.sh = {
#!/bin/sh

# Here, we assume the vdev has been configured with a 'loc' option set to
# so that it is found at the base addr of 0x1C0F0000.

# To stop the watchdog, first bring down any kicker utilities
slay -f wdtkick

# Then, write value of 0 to control register (base + 8) to disable it
io 32 0x1C0F0008 0

}

################################################################################################
## END OF BUILD SCRIPT
################################################################################################
]]])
changequote

