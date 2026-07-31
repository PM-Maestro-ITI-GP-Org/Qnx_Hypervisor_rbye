# Mosquitto Library - Quick Reference

## Steps

### Step 1: Download Mosquitto Source

```bash
cd /media/gemy/Extra/ITI_GP/Qnx_Hypervisor_rbye/src/mqtt_libs
./download_mosquitto.sh
```

### Step 2: Cross-Compile for QNX

```bash
./cross_compile_qnx.sh
```

This will:
- Configure mosquitto for QNX
- Compile the library
- Install to `install_qnx/` directory

### Step 3: Use the Library

The compiled library is in `install_qnx/`:
- Static library: `install_qnx/lib/libmosquitto.a`
- Shared library: `install_qnx/lib/libmosquitto.so`
- Headers: `install_qnx/include/mosquitto.h`

To link your program:

```bash
-DCMAKE_TOOLCHAIN_FILE=path/to/qt.toolchain.cmake \
-DQNX_LIB_DIR=/path/to/mqtt_libs/install_qnx/lib \
-I/path/to/mqtt_libs/install_qnx/include \
-lmosquitto
```

## Quick Commands

```bash
# Download source
./download_mosquitto.sh

# Cross-compile
./cross_compile_qnx.sh
```

## Troubleshooting

### QNX SDP not found
```bash
export QNX800_DIR=/media/gemy/Extra/ITI_GP/Qnx_Hypervisor_rbye/qnx800
. $QNX800_DIR/qnxsdp-env.sh
```