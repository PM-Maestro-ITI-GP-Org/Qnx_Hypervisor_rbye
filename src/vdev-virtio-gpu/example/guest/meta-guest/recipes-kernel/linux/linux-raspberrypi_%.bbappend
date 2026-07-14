# Turn the stock RPi kernel into a qvm virtio-gpu guest: virtio-mmio + DRM
# virtio-gpu + PL011 console. qvm auto-generates the guest FDT (virtio-mmio nodes)
# from the vdevs in the qvmconf; the bare-metal Pi defconfig enables none of this.
FILESEXTRAPATHS:prepend := "${THISDIR}/files:"

SRC_URI += "file://virtio-guest.cfg"
