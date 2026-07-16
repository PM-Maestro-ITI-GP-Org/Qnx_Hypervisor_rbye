#!/usr/bin/env bash
#
# Build + stage the vdev-virtio-gpu host-side dependencies and point the vdev at them.
#
#   libepoxy      (src/gpu/libepoxy)      -> build-qnx/src/libepoxy.so
#   virglrenderer (src/gpu/virglrenderer) -> build-qnx/src/libvirglrenderer.so
#   libscreen / libhypS                   <- this repo's QNX SDP (qnx800/)
#
# It assembles a self-contained stage dir (src/gpu/stage) with those libs + the pkg-config
# and meson cross files the two meson builds need, then writes src/gpu/vdev-virtio-gpu/
# paths.txt (VIRGL + STAGE) so `make -C gpu/vdev-virtio-gpu` links cleanly.
#
# Heavy meson builds run ONLY if their .so isn't present yet, and src/Makefile runs
# this whole script behind a stamp (.deps-built.stamp) so it does NOT run on every
# `make`. Force a full re-stage/rebuild with:  make -C src deps-clean && make -C src
#
set -eo pipefail

SRC_DIR="$(cd "$(dirname "$0")" && pwd)"          # .../src
REPO="$(cd "$SRC_DIR/.." && pwd)"
SDP_ENV="$REPO/qnx800/qnxsdp-env.sh"

# All GPU submodules live under src/gpu/ (vdev-virtio-gpu, virglrenderer, libepoxy).
GPU_DIR="$SRC_DIR/gpu"
VIRGL="$GPU_DIR/virglrenderer"
EPOXY="$GPU_DIR/libepoxy"
STAGE="$GPU_DIR/stage"
CROSS="$SRC_DIR/qnx-aarch64le.ini"
VDEV_PATHS="$GPU_DIR/vdev-virtio-gpu/paths.txt"

# --- fetch deps online (submodules) if a fresh clone hasn't checked them out -
for sm in virglrenderer libepoxy; do
  if [ ! -f "$GPU_DIR/$sm/meson.build" ]; then
    echo "==> fetching $sm (git submodule update --init)"
    git -C "$REPO" submodule update --init "src/gpu/$sm"
  fi
done

# --- QNX SDP env -----------------------------------------------------------
[ -f "$SDP_ENV" ] || { echo "ERROR: SDP env not found: $SDP_ENV" >&2; exit 1; }
# shellcheck disable=SC1090
source "$SDP_ENV" >/dev/null
[ -n "$QNX_HOST" ]   || { echo "ERROR: QNX_HOST unset after sourcing SDP env"   >&2; exit 1; }
[ -n "$QNX_TARGET" ] || { echo "ERROR: QNX_TARGET unset after sourcing SDP env" >&2; exit 1; }
TGT_USR_LIB="$QNX_TARGET/aarch64le/usr/lib"
TGT_LIB="$QNX_TARGET/aarch64le/lib"               # libhypS.a lives here (not usr/)
TGT_INC="$QNX_TARGET/usr/include"

echo "==> vdev deps: STAGE=$STAGE  VIRGL=$VIRGL"
mkdir -p "$STAGE/lib/pkgconfig" "$STAGE/include"

# --- meson cross file (generated from THIS repo's SDP) ---------------------
cat > "$CROSS" <<EOF
[binaries]
c = '$QNX_HOST/usr/bin/ntoaarch64-gcc'
cpp = '$QNX_HOST/usr/bin/ntoaarch64-g++'
ar = '$QNX_HOST/usr/bin/ntoaarch64-ar'
strip = '$QNX_HOST/usr/bin/ntoaarch64-strip'
pkgconfig = '/usr/bin/pkg-config'

[built-in options]
c_args = ['-D_QNX_SOURCE', '-include', '$QNX_TARGET/usr/include/sys/neutrino.h', '-DHAVE_TIMESPEC_GET=1']
c_link_args = []

[host_machine]
system = 'qnx'
cpu_family = 'aarch64'
cpu = 'aarch64'
endian = 'little'
EOF

# --- stage the SDP libraries the vdev links (symlinks stay current) --------
ln -sf "$TGT_USR_LIB/libscreen.so"    "$STAGE/lib/libscreen.so"
ln -sf "$TGT_USR_LIB/libscreen.so.1"  "$STAGE/lib/libscreen.so.1"
ln -sf "$TGT_LIB/libhypS.a"           "$STAGE/lib/libhypS.a"

# --- pkg-config files for the SDP libs the meson builds resolve ------------
emit_pc() {  # name  version  libflag  [extra-cflags]
  cat > "$STAGE/lib/pkgconfig/$1.pc" <<EOF
prefix=$QNX_TARGET/aarch64le/usr
exec_prefix=\${prefix}
libdir=\${exec_prefix}/lib
includedir=$TGT_INC

Name: $1
Description: $1 (QNX SDP)
Version: $2
Libs: -L\${libdir} $3
Cflags: -I\${includedir}${4:+ $4}
EOF
}
emit_pc libdrm 2.4.100 "-ldrm" "-I\${includedir}/libdrm"
emit_pc egl    1.5     "-lEGL"
emit_pc gl     1.0     "-lGLESv2"
emit_pc glesv2 3.2     "-lGLESv2"
emit_pc gbm    18.0.0  "-lgbm"

export PKG_CONFIG_LIBDIR="$STAGE/lib/pkgconfig"

# --- libepoxy (build only if missing) --------------------------------------
if [ ! -e "$EPOXY/build-qnx/src/libepoxy.so" ]; then
  echo "==> building libepoxy"
  make -C "$EPOXY" STAGE="$STAGE" MESON_CROSS="$CROSS" QNX_SDP_ENV="$SDP_ENV"
fi
cp -Pf "$EPOXY/build-qnx/src/libepoxy.so"     "$STAGE/lib/" 2>/dev/null || true
cp -Pf "$EPOXY/build-qnx/src/libepoxy.so.0"   "$STAGE/lib/" 2>/dev/null || true
cp -Pf "$EPOXY/build-qnx/src/libepoxy.so.0.0.0" "$STAGE/lib/" 2>/dev/null || true
mkdir -p "$STAGE/include/epoxy"
cp -f "$EPOXY"/include/epoxy/*.h "$STAGE/include/epoxy/" 2>/dev/null || true
# meson-generated epoxy headers (egl_generated.h / gl_generated.h) live under build-qnx
find "$EPOXY/build-qnx" -path '*/epoxy/*.h' -exec cp -f {} "$STAGE/include/epoxy/" \; 2>/dev/null || true
cat > "$STAGE/lib/pkgconfig/epoxy.pc" <<EOF
prefix=$STAGE
includedir=\${prefix}/include
libdir=\${prefix}/lib

epoxy_has_glx=0
epoxy_has_egl=1
epoxy_has_wgl=0

Name: epoxy
Description: GL dispatch library
Version: 1.5.11
Requires.private: egl, gl
Libs: -L\${libdir} -lepoxy
Cflags: -I\${includedir}
EOF

# --- virglrenderer (build only if missing) ---------------------------------
if [ ! -e "$VIRGL/build-qnx/src/libvirglrenderer.so" ]; then
  echo "==> building virglrenderer"
  make -C "$VIRGL" STAGE="$STAGE" MESON_CROSS="$CROSS" QNX_SDP_ENV="$SDP_ENV"
fi

# --- point the vdev at the staged deps -------------------------------------
cat > "$VDEV_PATHS" <<EOF
# Auto-generated by src/build-deps.sh -- do not edit. Regenerate with:
#   make -C src deps-clean && make -C src
# (paths.txt is gitignored in the vdev submodule.)
QNX_SDP_ENV=$SDP_ENV
VIRGL=$VIRGL
STAGE=$STAGE
EOF

echo "==> vdev deps ready -> $VDEV_PATHS"
