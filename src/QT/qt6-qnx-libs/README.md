# Qt 6.8.3 for QNX aarch64le

Cross-compile Qt libraries for QNX 8.0 (aarch64le) using Ubuntu host.

## Prerequisites

- QNX Software Platform 8.0 installed
- CMake ≥ 3.16
- `curl`, `tar`, `perl`, `whiptail`

## Reference

The general approach for cross-compiling Qt for aarch64 Linux (RPi) is described in:
https://medium.com/@janiethedev/cross-compile-qt-6-7-2-for-raspberry-pi-4-with-ubuntu-1e951af83bb5

The same principles apply here, but adapted for QNX:
- The **toolchain** is QCC (`qcc`/`q++`) instead of `aarch64-linux-gnu-g++`
- The **sysroot** is the QNX target directory (`$QNX_TARGET`) instead of a Raspbian sysroot
- Host tools (`moc`, `rcc`, …) are built as a native host Qt first, then used during the QNX cross-compile
- This project automates the entire flow via `scripts/configure.sh` + `scripts/download.sh` + `scripts/build.sh`

## Setup

Only three variables are needed:

```bash
export QT6_QNX_ENV=/path/to/qnxsdp-env.sh          # REQUIRED
export QT6_OUTPUT_DIR=/path/to/final/qt          # final installed libs
export QT6_WORKING_DIR=/path/to/scratch          # build scratch (host/ + qnx/)
```

Defaults (if not set):
- `QT6_OUTPUT_DIR` → `<project>/build_dir`
- `QT6_WORKING_DIR` → `<project>/build`  (host build at `build/host`, qnx build at `build/qnx`)

```bash
export QT6_QNX_ENV=/path/to/qnxsdp-env.sh
```

## Usage

```bash
# 1. Select which Qt modules to build
bash scripts/configure.sh

# 2. Download Qt source
bash scripts/download.sh

# 3. Build host Qt + cross-compile for QNX
bash scripts/build.sh
```

### Module selection

Opens an interactive checklist. Use Space to toggle, Enter to confirm.

Presets (non-interactive):
```bash
bash scripts/configure.sh --preset minimal   # qtbase + declarative + shadertools
bash scripts/configure.sh --preset default   # + svg + imageformats + multimedia
bash scripts/configure.sh --preset ivi       # + quick3d + websockets + charts + mqtt
bash scripts/configure.sh --preset full      # everything
```

## Scripts

### `scripts/configure.sh`
Interactive module selection. Shows a checklist of all detected Qt submodules; `qtbase`
is always built and shown as a non-editable header. Selected modules are saved to
`.build-config` as `SELECTED_MODULES`. On the next run it pre-selects the previous
choices. Also supports `--preset minimal|default|ivi|full` and `--text` (no GUI).

### `scripts/download.sh`
Downloads the full Qt 6.8.3 source tarball and extracts it into `qt6-source/`. Respects
`QT6_WORK_DIR` / `QT6_SOURCE_DIR` / `QT6_VERSION`.

**Manual download fallback:** If the download fails or is interrupted (slow connection,
timeout, etc.), you can download the archive manually from the Qt mirrorlist at:
https://download.qt.io/archive/qt/6.8/6.8.3/single/qt-everywhere-src-6.8.3.tar.xz.mirrorlist

Place the file `qt-everywhere-src-6.8.3.tar.xz` in the project root directory
(`<project>/qt-everywhere-src-6.8.3.tar.xz`). The download script checks for an existing
archive of at least 500 MB and skips the network download if one is found, extracting it
directly.

```bash
# Example: manually downloaded archive will be auto-detected
ls -lh qt-everywhere-src-6.8.3.tar.xz   # must be ≥ 500 MB
bash scripts/download.sh                 # detects local archive, skips download
```

### `scripts/build.sh`
Runs the full build in two stages:
- **`step1_build_host_qt`** — configures + builds + installs a native (host) Qt used to
  produce the tools (`moc`, `rcc`, `qmlcachegen`, …) required during cross-compilation.
  Installs to `QT6_OUTPUT_DIR/host_qt/`. Skipped if already present (set
  `QT6_FORCE_REBUILD=1` to force).
- **`step2_build_qnx_qt`** — cross-compiles Qt for `aarch64le` with the QNX toolchain,
  using `QT_HOST_PATH` → `host_qt/`. Installs to `QT6_OUTPUT_DIR/`. Between runs it
  cleans `QT6_OUTPUT_DIR` but deliberately **keeps** `host_qt/` (the QNX configure needs it).

> **Hint — cmake edit in `build.sh`:** `qtdeclarative` and `qtquick3d` call
> `include(${Qt6_DIR}/QtTargetHelpers.cmake)` and `QtBuildHelpers.cmake` **at configure
> time**. When `CMAKE_INSTALL_PREFIX` is set, Qt does *not* copy these two files into the
> build tree (it only installs them later), so configure fails. `build.sh` works around
> this with `pre_copy_helper_files()`, which copies both files from
> `qt6-source/qtbase/cmake/` into `<build>/qtbase/lib/cmake/Qt6/` for **both** the host
> and QNX build trees **before** running cmake. The Qt source tree is never modified —
> the copies live only in the scratch build dirs.

## Project structure

```
├── scripts/              # Build scripts
│   ├── configure.sh      # Module selection menu (writes .build-config)
│   ├── download.sh       # Downloads Qt tarball, extracts to qt6-source/
│   └── build.sh          # Host + QNX build (see "Scripts")
├── cmake/
│   └── toolchain_qnx_aarch64le.cmake   # QNX cross-compile toolchain
├── qt6-source/           # Qt source (after download) — never modified
├── build/                # Build SCRATCH (QT6_WORKING_DIR) — temp, safe to delete
│   ├── host/             # Host Qt cmake build tree + objects
│   └── qnx/              # QNX Qt cmake build tree + objects
├── build_dir/            # FINAL OUTPUT (QT6_OUTPUT_DIR)
│   ├── lib/              # .so files (aarch64)  ← deploy these to the QNX device
│   ├── qml/              # QML imports          ← deploy these too
│   ├── include/          # Headers (for building apps)
│   ├── bin/              # Qt tools (qmake, qt-cmake, ...)
│   └── host_qt/          # Host tools needed by the QNX build (moc, rcc, ...)
├── .build-config         # Module selection (generated by configure.sh)
└── README.md
```

**Separation of concerns:**
- `QT6_WORKING_DIR` (`build/`) holds only throwaway build artifacts. Deleting it just
  forces a rebuild.
- `QT6_OUTPUT_DIR` (`build_dir/`) holds the final, deployable Qt. This is what you copy
  to the QNX target (under `/qt`).

## Quick Build

Uses `Makefile` to automate the whole flow with smart skip logic.

### Default directories

| Variable | Default |
|----------|---------|
| `QT6_QNX_ENV` | source the qnx script before running make and it will set it |
| `QT6_OUTPUT_DIR` | `<project>/output_dir` |
| `QT6_WORKING_DIR` | `<project>/work_dir` |
| `QT6_SOURCE_DIR` | `<project>/qt6-source` |

### Quick start

```bash
# 1. Source QNX environment (required)
source ../../../qnx800/qnxsdp-env.sh

# 2. Configure modules (first time only)
make config

# 3. Build everything (download + compile)
make

# 4. Check status
make status
```

### Targets

| Target | Description |
|--------|-------------|
| `make` | Download + build (skips if already done) |
| `make config` | Open module selection menu |
| `make download` | Download source only |
| `make build` | Build only |
| `make rebuild` | Clean and rebuild from scratch |
| `make status` | Show paths and selected modules |
