#!/bin/ksh
#
# Bring up the paravirtual GPU inside the QNX guest (virtio-gpu over MMIO).
#
# Talks to the host-side vdev-virtio-gpu.so (see src/vdev-virtio-gpu/). No PCI:
# drm-virtio attaches to the virtio-gpu on the MMIO transport, then QNX Screen
# runs the Mesa virgl stack on top (guest GL -> SUBMIT_3D -> host virglrenderer).
#
# The host vdev (qnx-guest-1.qvmconf) places the virtio-gpu at:
#     loc  0x1c0e0000   (MMIO register window base)
#     intr gic:39       (guest GIC interrupt id)
# drm-virtio is a Linux-6.12 DRM shim; the virtio-mmio device is instantiated via
# the virtio module param  virtio.device=<size>@<addr>:<intr>  fed through "-o"
# (kernel-module options). The param MUST be module-qualified as "virtio.device":
# drm-virtio's own hint prints a bare "device=" which set_module_param rejects with
# "Trying to set unknown param device". 0x1000 = std virtio-mmio window; 39 = gic:39.
#
# NOTE: the HOST must already have QNX Screen running before qvm starts this guest
# (the vdev gates EGL on the host's screen_create_context) -- a host-side concern.

GPU_LOC=0x1c0e0000
GPU_SIZE=0x1000
GPU_INTR=39

echo "Starting drm-virtio (MMIO virtio-gpu @ ${GPU_LOC}, intr ${GPU_INTR}) ..."

# drm-virtio links libpci for an optional PCI pre-init; with no /dev/pci it just
# prints "No PCI buses found, skipping pre-init" and continues on the MMIO path.
drm-virtio -o virtio.device=${GPU_SIZE}@${GPU_LOC}:${GPU_INTR} &

# If it still won't bind, run "drm-virtio" with no args to have it list the MMIO
# candidates it discovered in the DTB, e.g.:
#     VirtIO MMIO devices (i.e. candidates for 'device' parameter):
#         virtio-mmio.N: 0x1000@0x1c0e0000:39
# then feed that "<size>@<addr>:<intr>" into virtio.device= above. Other tunables
# on the same module if needed: virtio.force_legacy=0, virtio.modeset=1.

if ! waitfor /dev/dri/card0 10; then
    echo "ERROR: /dev/dri/card0 did not appear -- drm-virtio failed to bind the vdev."
    echo "       Check the host vdev loaded and the loc/intr match the qvmconf."
    exit 1
fi
echo "  /dev/dri/card0 up"

echo "Starting QNX Screen (graphics-virtio-mmio.conf) ..."
# The FIRST screen launch right after /dev/dri/card0 appears can lose an EGL/capset
# race with the freshly-attached virtio-gpu and exit with "cmContextInit failed".
# A clean relaunch then succeeds, so retry a few times before giving up.
# Do NOT set MESA_LOADER_DRIVER_OVERRIDE here -- the package's default virtio_dri.so
# is correct; forcing "virpipe" yields "DRI2: unsupported DRI driver".
attempt=0
while [ $attempt -lt 5 ]; do
    slay -f screen 2>/dev/null
    screen -c /usr/share/screen/graphics-virtio-mmio.conf
    if waitfor /dev/screen 5; then
        break
    fi
    attempt=$((attempt + 1))
    echo "  screen not up (attempt ${attempt}/5), retrying ..."
done

if ! waitfor /dev/screen 2; then
    echo "ERROR: /dev/screen did not appear after 5 attempts -- screen failed to start."
    exit 1
fi
echo "  /dev/screen up -- GPU ready. Try:  gles2-gears"
