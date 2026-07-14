# virglrenderer QNX port

↑ [Main README](../README.md) · used by the vdev [`Makefile`](../Makefile) and
[`build-image.sh`](../build-image.sh)

`virglrenderer-qnx.patch` is the QNX port of virglrenderer that the vdev builds
and links against (the `VIRGL` dir in `paths.txt`). It is the *entire* port — 5
small source changes — so the checkout referenced by `paths.txt` must have it
applied, and the prebuilt `libvirglrenderer.so.1` in the host IFS was compiled
with it.

## What it changes

| File | Change |
|---|---|
| `src/mesa/util/detect_os.h` | add `DETECT_OS_QNX` (+ `DETECT_OS_UNIX`) for `__QNXNTO__` |
| `src/mesa/util/os_misc.c` | add `DETECT_OS_QNX` to the `unistd.h` / `sysconf` `#if`s |
| `src/vrend/vrend_winsys.c` | **skip `virgl_gbm_init` when surfaceless with no caller fd** — QNX's gbm/dri loader path crashes (`MESA-LOADER: failed to retrieve device information`); pure-EGL surfaceless works |
| `src/vrend/vrend_winsys_egl.c` | guard the gbm branch with `egl->gbm &&` (NULL deref otherwise) |
| `src/vrend/vrend_winsys_gbm.c` | wrap the `d_type != DT_CHR` filter in `#ifdef DT_CHR` (QNX dirent has no `d_type`) |

## Apply

Base commit: **`f849f8e1cd4eb68a3eee17950602362618b6816c`**
(`drm: Perform a bounds check immediately before copying to shmem`, branch `main`).

```sh
git clone https://gitlab.freedesktop.org/virgl/virglrenderer.git
cd virglrenderer
git checkout f849f8e1cd4eb68a3eee17950602362618b6816c
git apply /path/to/QNX_vdev-Virtio-GPU/patches/virglrenderer-qnx.patch
```

Point `VIRGL` in `paths.txt` at this checkout.

## Cross-build for QNX (aarch64le)

External prerequisites (not in this repo): the QNX SDP, a **libepoxy** cross-built
and staged into `STAGE`, a meson cross-file using the *raw* GCC
(`aarch64-unknown-nto-qnx8.0.0-gcc`, not the `qcc` wrapper — meson can't parse
qcc's version banner), and hand-written pkg-config `.pc` stubs in
`STAGE/lib/pkgconfig` (QNX ships none): `gbm`, `libdrm`, and `egl`/`gl`/`glesv2`.

```sh
source $QNX_SDP_ENV
export PKG_CONFIG_LIBDIR=$STAGE/lib/pkgconfig        # LOCK to QNX-only .pc files

cd $VIRGL
meson setup build-qnx --cross-file <qnx-aarch64le.ini> \
  --prefix $STAGE \
  -Dplatforms=egl -Drender-server-worker=thread -Dc_link_args="-lm -leventfd"
ninja -C build-qnx src/libvirglrenderer.so.1.11.0    # lib target only (vtest won't build on QNX)

# virglrenderer links libdrm.so.1 (default -ldrm) but QNX's v3d_dri/EGL-mesa use
# libdrm.so.2; both loaded -> wrong bind -> SIGSEGV in eglInitialize. Repoint it:
patchelf --replace-needed libdrm.so.1 libdrm.so.2 build-qnx/src/libvirglrenderer.so.1.11.0
```

`build-qnx/src/` then holds the headers (`virgl-version.h`) and
`libvirglrenderer.so.1*` that the vdev `Makefile` includes and links. The same
`.so` is copied into the host IFS as the runtime `libvirglrenderer.so.1`.

Runtime note: QNX has no `EGL_PLATFORM_SURFACELESS_MESA`, so the patched build
falls back to `eglGetDisplay(EGL_DEFAULT_DISPLAY)` = the Screen display — QNX
Screen must be running and `screen_create_context()` must be called before any
EGL call (see the [main README](../README.md)'s render-thread notes).
