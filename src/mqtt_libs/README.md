# Mosquitto MQTT Library for QNX

This directory contains scripts to download and cross-compile mosquitto MQTT library for QNX.

## Quick Start

All paths in the scripts are relative to this directory, so it can live/move anywhere.

1. **Download mosquitto source**:
   ```bash
   ./download_mosquitto.sh
   ```

2. **Cross-compile for QNX**:
   ```bash
   ./cross_compile_qnx.sh
   ```

Or run both from the Makefile:

```bash
make download      # download source only
make compile       # cross-compile only
make install       # download + compile (default)
```

## How It Works

### Script: `download_mosquitto.sh`

Downloads mosquitto source code from GitHub and extracts it to:
- Source: `mosquitto-2.0.20/`
- Version: 2.0.20 (latest stable at time of creation)

Uses:
- `git clone` if git is available
- Tarball download if git is not available

### Script: `cross_compile_qnx.sh`

1. Sources QNX SDP environment from `qnx800/qnxsdp-env.sh`
2. Configures mosquitto for QNX target using:
   - `--host=$QNX_HOST` (aarch64le)
   - `--prefix="$INSTALL_DIR"` (install directory)
   - `--disable-tests` (skip tests for faster build)
   - `--disable-crypto` (no OpenSSL/MbedTLS needed)
   - `--with-static-libraries` (build static library)
3. Compiles with 4 parallel jobs
4. Installs to `install_qnx/` directory

## Files

```
mqtt_libs/
├── download_mosquitto.sh      # Download source
├── cross_compile_qnx.sh       # Cross-compile for QNX
├── README.md                   # This file
├── mosquitto-2.0.20/          # Source code (after download)
├── build_qnx/                 # Build output
└── install_qnx/               # Installation
    ├── lib/
    │   ├── libmosquitto.a     # Static library
    │   └── libmosquitto.so    # Shared library
    └── include/
        └── mosquitto.h        # Headers
```

## Using the Library

To link your program with mosquitto:

```bash
cmake -DCMAKE_TOOLCHAIN_FILE=path/to/qt.toolchain.cmake \
     -DQNX_LIB_DIR=/path/to/mqtt_libs/install_qnx/lib \
     -I/path/to/mqtt_libs/install_qnx/include \
     -lmosquitto
```

Or compile manually:

```bash
qcc -Vgcc_ntoaarch64le -std=c11 \
    -I/mqtt_libs/install_qnx/include \
    -L/mqtt_libs/install_qnx/lib \
    -lmosquitto \
    -o myapp myapp.c
```

## Troubleshooting

### QNX SDP not found
```bash
export QNX800_DIR=/media/gemy/Extra/ITI_GP/Qnx_Hypervisor_rbye/qnx800
. $QNX800_DIR/qnxsdp-env.sh
```

### Compilation errors
Make sure you have:
- QNX SDP installed and sourced
- CMake and GNU toolchain installed
- Sufficient disk space (~100MB)

### License

Mosquitto: EPL 1.0 / BSD 3-Clause (https://mosquitto.org/license/)

## Version

Mosquitto version: 2.0.20
MQTT protocol: 3.1.1 (with 5.0 support)
Cross-platform: QNX, Linux, Windows
