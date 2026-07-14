#!/usr/bin/env bash
# Build the flashable RPi5 SD image for the vdev-virtio-gpu demo:
#   host QNX hypervisor (gears on screen 2) + Linux guest (kmscube on screen 1).
#
# Builds the two things this repo owns from source (the vdev .so and the guest
# image), stages them into the QNX host build tree, and assembles disk.img.
# Everything outside the repo (QNX SDP, virgl, Yocto, the host IFS build tree,
# the BSP) comes from paths.txt.
#
#   cp paths.txt.example paths.txt && edit it, then:  ./build-image.sh
#   SKIP_GUEST=1 ./build-image.sh     # reuse the already-built guest image
set -euo pipefail

REPO="$(cd "$(dirname "$0")" && pwd)"
[ -f "$REPO/paths.txt" ] || { echo "ERROR: no paths.txt — copy paths.txt.example and fill it in."; exit 1; }
# shellcheck disable=SC1091
source "$REPO/paths.txt"

# Fail early with a clear message if any path is unset or missing.
for v in QNX_SDP_ENV VIRGL STAGE YOCTO_POKY HYP_DIR BSP_INSTALL; do
	p="${!v:-}"
	[ -n "$p" ] || { echo "ERROR: $v not set in paths.txt"; exit 1; }
	[ -e "$p" ] || { echo "ERROR: $v points at a missing path: $p"; exit 1; }
done

# shellcheck disable=SC1090
source "$QNX_SDP_ENV"

echo "=== [1/3] vdev-virtio-gpu.so (host) ==="
make -C "$REPO" VIRGL="$VIRGL" STAGE="$STAGE" clean
make -C "$REPO" VIRGL="$VIRGL" STAGE="$STAGE"
# The IFS bakes in the vdev from the host tree; refresh it so mkifs picks this build.
cp -f "$REPO/vdev-virtio-gpu.so" "$HYP_DIR/vdev-virtio-gpu/vdev-virtio-gpu.so"

echo "=== [2/3] guest image (Linux) ==="
if [ "${SKIP_GUEST:-0}" != 1 ]; then
	( set +u; source "$YOCTO_POKY/oe-init-build-env" "$YOCTO_BUILD" >/dev/null && bitbake guest-image )
fi
DEPLOY="$YOCTO_BUILD/tmp/deploy/images/raspberrypi5"
cp -f  "$DEPLOY/Image" "$HYP_DIR/guests/linux-guest/Image"
cp -fL "$DEPLOY/guest-image-raspberrypi5.rootfs.cpio.gz" "$HYP_DIR/guests/linux-guest/rootfs.cpio.gz"

echo "=== [3/3] assemble disk.img (host IFS + guest data partition) ==="
# The host Makefile's part_qnx_data.img rule doesn't depend on guests/ contents,
# so drop the stale outputs to force a rebuild with the freshly-staged guest.
rm -f "$HYP_DIR/part_qnx_data.img" "$HYP_DIR/disk.img" "$HYP_DIR/disk.bmap"
make -C "$HYP_DIR" BOARD=rpi5 INSTALL="$BSP_INSTALL" disk.img disk.bmap

# The host Makefile emits into its own tree; move the outputs into this repo.
mkdir -p "$REPO/build"
mv -f "$HYP_DIR/disk.img" "$HYP_DIR/disk.bmap" "$REPO/build/"

echo
echo "=== DONE ==="
echo "Image:  $REPO/build/disk.img"
echo "Bmap:   $REPO/build/disk.bmap"
echo "Flash:  sudo dd if=$REPO/build/disk.img of=/dev/sdX bs=4M conv=fsync status=progress && sync"
