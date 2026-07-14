# QtQuick2DDemo

Test application for the cross-compiled Qt 6.8.3 for QNX aarch64le.

A simple 2D animation demo with bouncing shapes, text display, and interactive input.

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
  -DQNX_LIB_DIR=../../../qnx800/target/qnx/aarch64le

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
