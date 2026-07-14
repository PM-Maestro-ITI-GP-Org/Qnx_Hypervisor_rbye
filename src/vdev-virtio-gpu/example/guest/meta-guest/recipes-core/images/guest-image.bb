SUMMARY = "Minimal Linux guest for the QNX qvm virtio-gpu demo - autostarts kmscube"
DESCRIPTION = "Tiny arm64 initramfs (cpio.gz) for qvm 'initrd load'. Just enough to \
render a kmscube through the paravirtual virtio-gpu (Mesa virgl on the host V3D) \
and page it out to the guest's single DRM device (/dev/dri/card0 = HDMI1)."

LICENSE = "MIT"

inherit core-image

# Delivered to qvm as the initial ramdisk; qvmconf uses rdinit=/sbin/init.
IMAGE_FSTYPES = "cpio.gz"

# The whole point: Mesa virgl driver + GBM + kmscube, plus the autostart unit.
IMAGE_INSTALL:append = " \
    kmod \
    libdrm \
    mesa-megadriver \
    libgbm \
    kmscube \
    kmscube-autostart \
"

# Keep the ramdisk small (unpacked into guest RAM).
INITRAMFS_MAXSIZE = "262144"
IMAGE_ROOTFS_EXTRA_SPACE = "8192"
