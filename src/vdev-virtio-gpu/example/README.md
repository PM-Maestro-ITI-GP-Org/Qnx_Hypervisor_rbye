# Demo: gears on screen 2 (host) + kmscube on screen 1 (guest)

↑ [Main README](../README.md) · builds via [`build-image.sh`](../build-image.sh) ·
virgl port: [`patches/README.md`](../patches/README.md)

Two GL apps on two HDMIs sharing one RPi5 GPU:

- **Screen 2 (HDMI2)** — QNX host runs `gles2-gears`. Proves the host owns the V3D.
- **Screen 1 (HDMI1)** — Linux guest runs `kmscube` through the `vdev-virtio-gpu`
  backend: guest Mesa **virgl** → host **virglrenderer** → host **V3D**, scanned
  out by QNX Screen. `GL_RENDERER` in the guest reads `virgl`.

```
  QNX host (owns V3D + both HDMIs)
  ├── gles2-gears -display=2 ───────────────► HDMI2 (screen 2)
  └── qvm guest.qvmconf
        └── vdev-virtio-gpu.so ──virgl──► V3D ─► HDMI1 (screen 1)
              ▲
        Linux guest: kmscube on /dev/dri/card0
```

## host/
- `guest.qvmconf` — minimal qvm config with one `virtio-gpu` vdev pinned to
  `scanout-display 1` (HDMI1). Set `scanout-width/height` to your panel.
- `start-demo.sh` — waits for `/dev/screen`, launches gears on screen 2, then the
  guest. Call it from the IFS startup script for autostart, or run by hand.

Needs on the host: the built `vdev-virtio-gpu.so` (from the repo root) plus the
patched `libvirglrenderer`/`libepoxy` reachable via `LD_LIBRARY_PATH`, and the
guest `Image` + `rootfs.cpio.gz` next to `guest.qvmconf`.

## guest/meta-guest/
Minimal Yocto layer (scarthgap) — only what kmscube needs:
- `recipes-kernel/linux` — enables virtio-mmio + `DRM_VIRTIO_GPU` + PL011.
- `recipes-graphics/mesa` — adds the `virgl` gallium driver.
- `recipes-core/kmscube` — `kmscube-autostart` sysvinit script (S99, runlevel 5).
- `recipes-core/images/guest-image.bb` — cpio.gz initramfs: mesa + kmscube + autostart.

Build: add the layer, then `bitbake guest-image`; copy `Image` and
`guest-image-*.cpio.gz` (as `rootfs.cpio.gz`) to `host/` on the target.
