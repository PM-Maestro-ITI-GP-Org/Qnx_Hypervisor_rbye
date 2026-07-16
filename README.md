# QNX 8.0 Hypervisor on Raspberry Pi 5

A QNX 8.0 hypervisor (`qvm`) system for the Raspberry Pi 5: a QNX **host** that owns the
hardware (V3D GPU, HDMI, SPI, GPIO, networking) and boots one or more QNX **guests**.
Guest-1 gets a **paravirtual GPU** — its OpenGL ES runs through a `virtio-gpu` device over
MMIO, is executed by `virglrenderer` on the host's real V3D, and is scanned out by QNX Screen.

```
  Raspberry Pi 5  ─ QNX 8.0 host (owns V3D + HDMI + SPI/GPIO)
  ├── QNX Screen (drm-rpi5)  ──────────────────────► HDMI
  └── qvm @qnx-guest-1.qvmconf
        └── vdev-virtio-gpu.so ── virgl ──► virglrenderer ──► V3D ──► HDMI
              ▲ virtio-mmio (no PCI)
        QNX guest-1: drm-virtio → /dev/dri/card0 → Screen → gles2-gears / Qt
```

---

## 1. Repo layout

| Path | What it is |
|---|---|
| `qnx800/` | QNX SDP 8.0 — toolchain (`qcc`) + target libs + image tools (`mkifs`, `mkqnx6fsimg`, `diskimage`) |
| `qnx_host/` | The hypervisor host. `images/rpi5-hypervisor.build` = host IFS; `images/part_qnx_data.build` = data partition (holds the guests + big assets); `images/disk.cfg` → `disk.img` |
| `qnx_guests/` | Guest images: `images/guest-1/`, `images/guest-2/` — each has a `.build` (IFS contents) and a `.qvmconf` (virtual machine config) |
| `src/` | Applications + the GPU stack (see below) |
| `src/gpu/` | **Git submodules**: `vdev-virtio-gpu` (the qvm GPU backend), `virglrenderer`, `libepoxy` |
| `conf/display/` | QNX Screen configs + graphics startup scripts (host and guest) |
| `utils/` | Misc helper scripts |

---

## 2. Prerequisites (build machine — Linux)

The QNX SDP is bundled in `qnx800/`. Everything else must be on your Linux host:

| Tool | Why |
|---|---|
| `make`, `git` | Build + submodules |
| `meson`, `ninja`, `python3` | Build `virglrenderer` and `libepoxy` |
| **`patchelf`** | **Required.** `virglrenderer`'s build rewrites `libdrm.so.1` → `libdrm.so.2`. Without it you get a `SIGSEGV` inside `eglInitialize` at runtime |
| `pkg-config` | meson dependency resolution for the GPU libs |
| `sudo` + `dd` | Flashing the SD card |
| `sshpass` | Optional — convenient `scp` deploys to the running board |

Install on Debian/Ubuntu:

```sh
sudo apt install make git meson ninja-build python3 patchelf pkg-config sshpass
```

> If you need to (re)install the QNX SDP packages into `qnx800/`, see `make qnx_install` below.

---

## 3. Getting the source

The GPU stack lives in submodules, so clone recursively:

```sh
git clone --recurse-submodules <this-repo-url>
cd Qnx_Hypervisor_rbye
```

Already cloned without them? Fetch them with:

```sh
git submodule update --init --recursive
```

(`src/build-deps.sh` also auto-fetches `virglrenderer`/`libepoxy` if they're missing.)

---

## 4. Building

**Source the SDP environment first — every build needs it:**

```sh
source qnx800/qnxsdp-env.sh     # sets QNX_HOST / QNX_TARGET, puts qcc + mkifs on PATH
make                            # builds everything
```

`make` builds in dependency order: **apps + GPU deps → guest images → host image + `disk.img`**.

The first build is slow: `src/build-deps.sh` compiles `libepoxy` and `virglrenderer` with meson,
stages them (plus `libscreen`/`libhypS` from the SDP) into `src/gpu/stage`, generates the meson
cross file, and writes `src/gpu/vdev-virtio-gpu/paths.txt` so the vdev links. This runs **once** —
it's guarded by `src/.deps-built.stamp` and won't repeat on later builds.

### Make targets — root

| Target | What it does |
|---|---|
| `make` / `make all` | Everything: apps → guests → host image |
| `make apps_src` | Just `make -C src` (GPU deps + vdev + all apps) |
| `make qnx_guests` | Guest IFS images (builds apps first) |
| `make qnx_host` | Host IFS + `disk.img` (builds apps + guests first) |
| `make qnx_host/images/disk.img` | Just the flashable SD image |
| `make flash` | Build `disk.img` and write it to an SD card (see below) |
| `make qnx_install` | (Re)install QNX SDP packages into `qnx800/` via `qnxsoftwarecenter_clt`. Requires `QSC_CLT_PATH=/path/to/qnxsoftwarecenter_clt` and `TARGET=<dir containing qsc_install_packages.list>` |
| `make clean` | Clean `qnx_host`, `qnx_guests`, and `src` |

### Make targets — `src/`

| Target | What it does |
|---|---|
| `make -C src` | GPU deps (once) → `gpu/vdev-virtio-gpu` → every app directory |
| `make -C src deps` | Only build/stage the GPU dependencies |
| `make -C src deps-clean` | Drop the stamp so the deps re-stage on the next build |
| `make -C src clean` | Clean all apps, the GPU submodules, and `src/gpu/stage` |

---

## 5. Flashing the SD card

```sh
make flash FLASH_DEVICE=/dev/sdX      # or just `make flash` and it will prompt
```

> ⚠️ **This `dd`s over the whole device.** Confirm the path with `lsblk` first — picking the
> wrong one will destroy your system disk. It runs `sudo dd … conv=fsync`.

Use `dd` (a full write), **not** `bmaptool`: a bmap reflash of a reused card can leave stale
qnx6fs metadata and corrupt the data partition.

---

## 6. Running

Boot the Pi. The host's boot script brings up networking, SSH, SPI/GPIO, and **host graphics**
automatically (`ksh /scripts/host-graphics-start.sh` → QNX Screen on the V3D).

The board comes up on **`192.168.2.2`** (`root` / `root`).

### Start a guest (on the host)

```sh
cd /guests/guest-1
qvm @qnx-guest-1.qvmconf
```

> **Host Screen must already be running before `qvm` starts.** The GPU vdev gates its EGL
> context on the host's `/dev/screen`; an EGL call before Screen is up kills the whole `qvm`
> process. The boot script handles this ordering for you.

### Inside the guest

```sh
ksh /scripts/graphics-virtio-start.sh   # drm-virtio (MMIO) → /dev/dri/card0, then Screen
gles2-gears                             # GL through the paravirtual GPU
sh /QT_Demo_APP/run.sh                  # the Qt demo app
```

(Guest graphics is started manually. To auto-start it at boot, uncomment the
`ksh /scripts/graphics-virtio-start.sh` line in `customize_startup.sh` inside
`qnx800-guest-1.build`.)

### The resolution rule

The GPU vdev scans the guest out **1:1 with no scaling**. These three **must** be equal (and
match the physical panel), or the guest will render into only a corner of the display:

| Where | Setting |
|---|---|
| `qnx_guests/images/guest-1/qnx-guest-1.qvmconf` | `scanout-width` / `scanout-height` |
| `conf/display/graphics-virtio-mmio.conf` (guest) | `video-mode` |
| `conf/display/graphics-host-rpi5.conf` (host, display 1) | `video-mode` |

Currently all three are **1024 x 600**.

### Fast dev loop (no reflash)

Guest changes don't need a card reflash — the guest IFS and its qvmconf live on the host's
data partition:

```sh
scp qnx_guests/images/guest-1/qnx800-guest-1.ifs    root@192.168.2.2:/guests/guest-1/qnx-guest.ifs
scp qnx_guests/images/guest-1/qnx-guest-1.qvmconf   root@192.168.2.2:/guests/guest-1/
# then on the host:  slay qvm  &&  qvm @qnx-guest-1.qvmconf
```

Only **host IFS** changes require rebuilding `disk.img` and reflashing.

---

## 7. Adding a new application to `src/`

**Step 1 — create the app.** Make `src/<myapp>/` with your sources and a `Makefile` that
cross-compiles with `qcc` into a predictable output path:

```make
# src/myapp/Makefile
QCC       = qcc
VARIANT  ?= gcc_ntoaarch64le
CFLAGS    = -Wall -Wextra -O2
BUILD_DIR = build

.PHONY: all clean
all: $(BUILD_DIR)/myapp

$(BUILD_DIR):
	mkdir -p $@

$(BUILD_DIR)/myapp: myapp.c | $(BUILD_DIR)
	$(QCC) -V$(VARIANT) $(CFLAGS) -o $@ myapp.c

clean:
	rm -rf $(BUILD_DIR)
```

Provide `all` and `clean` targets — the build system relies on both.

**Step 2 — nothing to register.** `make -C src` auto-discovers any directory containing a
`Makefile`/`GNUmakefile` and builds it (`src/gpu/` is skipped; it's handled by `build-deps.sh`).

**Step 3 — put it in an image.** Add a line mapping *destination-in-image* → *source-on-disk*:

*Guest* (`qnx_guests/images/guest-1/qnx800-guest-1.build`):
```
myapp=../../../src/myapp/build/myapp
```

*Host IFS* (`qnx_host/images/rpi5-hypervisor.build`):
```
/bin/myapp=../../src/myapp/build/myapp
```

*Host data partition*, for large assets (`qnx_host/images/part_qnx_data.build`):
```
/myapp_data=../../src/myapp/data
```

**Step 4 — rebuild:** `make`. The guest IFS auto-rebuilds when any file it stages changes.

### `.build` file gotchas

- **Never put `}` or `${...}` inside an inline `= { }` block.** `mkifs` ends the block at the
  *first* `}` — including one inside a shell `${var}` or even a comment — and then parses the
  rest of your script as build directives. Put scripts in a real file and reference them:
  ```
  [perms=0755] /scripts/myapp.sh=../../../conf/myapp.sh
  ```
- **A guest IFS is RAM-resident.** Everything you stage into it consumes the guest's `ram` from
  the qvmconf. Large payloads (e.g. the ~188 MB Qt deploy) need a matching RAM bump, or should
  live on the host's data partition instead.
- **Whole directories** use a trailing slash on the destination:
  ```
  QT_Demo_APP/ =../../../src/QT/Demo_APP/build_qnx/deploy
  ```

---

## 8. Troubleshooting

| Symptom | Cause / fix |
|---|---|
| `SIGSEGV` in `eglInitialize` on the host | `patchelf` wasn't installed when `virglrenderer` was built. Install it, then `make -C src deps-clean && make -C src` |
| Whole `qvm` process exits instantly, no error | An EGL call ran before host Screen was up. Start host graphics before `qvm` |
| Guest GL renders in only a corner of the screen | Resolution mismatch — see *The resolution rule* above |
| Guest `screen -c …` fails: "library cannot be found" | A *dependency* of the named `.so` is missing (dlopen reports the top-level file). Run `ldd` on it **in the guest** to find the real culprit |
| `mkifs: Improper filename specification` / `Host file 'x' not available` | A `}` leaked out of an inline `= { }` block — see the `.build` gotchas |
| Guest image didn't pick up a changed file | Should be automatic; force it with `make -C qnx_guests` or delete the `.ifs` |
