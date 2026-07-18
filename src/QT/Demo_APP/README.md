# QtQuick2DDemo

Test application for the cross-compiled Qt 6.8.3 for QNX aarch64le.

A simple 2D animation demo with bouncing shapes, text display, and interactive input.

## How it works

**`CMakeLists.txt`** — builds the app binary, then runs `deploy_qt.cmake` as a post-build step.

**`deploy_qt.cmake`** produces a self-contained `deploy/` directory with only the required dependencies:

1. **QNX platform plugin** — `libqqnx.so` from Qt plugins
2. **QML modules** — `QtQuick`, `QtQml`, `QtQuick.Controls` from Qt qml dir
3. **Fonts** — reads `fonts.txt`, finds each font in `FONT_SOURCE_DIR`, copies to `deploy/lib/fonts/`, then generates a minimal `fonts.conf`
4. **Recursive dependency resolution** — walks the app binary + every `.so` in `lib/`, `qml/`, `plugins/`, reads `NEEDED` entries via `objdump`/`readelf`, and pulls any missing libs from the Qt lib dir / QNX SDK. Repeats until the dependency set is closed. This auto-discovers all required Qt and system libs — no hardcoded lists.
5. **Broken symlink cleanup** — removes dangling symlinks so the image builder won't fail

The result is a lean, closed set of dependencies with no static `.a` or symbol `.sym` files.

### Fonts

Fonts listed in `fonts.txt` are copied from `FONT_SOURCE_DIR` (recursive search — finds fonts in nested subdirectories). Defaults to `/usr/share/fonts`. A minimal `fonts.conf` is generated so fontconfig doesn't complain at runtime.

```bash
# Use system fonts (default)
cmake -S . -B build_qnx -DFONT_SOURCE_DIR=/usr/share/fonts

# Or a custom directory
cmake -S . -B build_qnx -DFONT_SOURCE_DIR=/path/to/my/fonts
```

## Build native (test on host)

```bash
cmake -S . -B build -DQt6_DIR=../qt6-qnx-libs/output_dir/host_qt/lib/cmake/Qt6
cmake --build build
./build/deploy/QtQuick2DDemo
```

## Cross-build for QNX

```bash
source ../../../qnx800/qnxsdp-env.sh

cmake -S . -B build_qnx \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE=../qt6-qnx-libs/output_dir/lib/cmake/Qt6/qt.toolchain.cmake \
  -DQt6_DIR=../qt6-qnx-libs/output_dir/lib/cmake/Qt6 \
  -DQNX_LIB_DIR=../../../qnx800/target/qnx/aarch64le \
  -DFONT_SOURCE_DIR=/usr/share/fonts

cmake --build build_qnx
```

## Run on QNX

```bash
# Set screen size (optional)
export QQNX_PHYSICAL_SCREEN_SIZE=150,90

# Run with software backend
./build_qnx/deploy/run.sh
```

Set window size via env vars:
```bash
APP_WIDTH=1920 APP_HEIGHT=1080 ./build_qnx/deploy/run.sh
```

## Quick Build (Makefile)

```bash
# Native
make native

# QNX (source QNX first)
source ../../../qnx800/qnxsdp-env.sh
make qnx

# Default builds QNX
make
```
