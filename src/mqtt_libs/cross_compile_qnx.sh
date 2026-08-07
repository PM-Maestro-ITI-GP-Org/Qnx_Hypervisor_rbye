#!/bin/bash
# Cross-compile mosquitto for QNX using CMake

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

MOSQUITTO_VERSION="2.0.20"
SOURCE_DIR="${SCRIPT_DIR}/mosquitto-${MOSQUITTO_VERSION}"
BUILD_DIR="${SCRIPT_DIR}/build_qnx"
INSTALL_DIR="${SCRIPT_DIR}/install_qnx"
QNX800_DIR="${QNX800_DIR:-${SCRIPT_DIR}/../../qnx800}"
CJSON_DIR="${SCRIPT_DIR}/cJSON"

echo "=========================================="
echo "Mosquitto Cross-Compilation for QNX"
echo "=========================================="
echo ""

# Check if source exists
if [ ! -d "$SOURCE_DIR" ]; then
    echo "Error: Mosquitto source not found at: $SOURCE_DIR"
    echo "Please run download_mosquitto.sh first"
    exit 1
fi

# Source QNX environment
echo "Loading QNX SDP environment..."
if [ ! -f "$QNX800_DIR/qnxsdp-env.sh" ]; then
    echo "Error: QNX SDP not found at: $QNX800_DIR/qnxsdp-env.sh"
    exit 1
fi

. "$QNX800_DIR/qnxsdp-env.sh"

echo "QNX_HOST: $QNX_HOST"
echo "QNX_TARGET: $QNX_TARGET"
echo ""

# Verify QNX compiler exists
if [ ! -f "$QNX_HOST/usr/bin/qcc" ]; then
    echo "Error: QCC compiler not found at $QNX_HOST/usr/bin/qcc"
    exit 1
fi
echo "QCC compiler: $QNX_HOST/usr/bin/qcc"
echo ""

# ==========================================
# Step 1: Build cJSON dependency
# ==========================================
echo "--- Building cJSON dependency ---"
if [ ! -d "$CJSON_DIR" ]; then
    echo "Cloning cJSON..."
    cd "$(dirname "$CJSON_DIR")"
    git clone --depth 1 https://github.com/DaveGamble/cJSON.git
fi

CJSON_BUILD_DIR="$BUILD_DIR/cJSON"
CJSON_INSTALL_DIR="$INSTALL_DIR/cJSON"
mkdir -p "$CJSON_BUILD_DIR"
mkdir -p "$CJSON_INSTALL_DIR"

cat > "$CJSON_BUILD_DIR/qnx-toolchain.cmake" << EOF
set(CMAKE_SYSTEM_NAME QNX)
set(CMAKE_SYSTEM_PROCESSOR aarch64)
set(CMAKE_C_COMPILER $QNX_HOST/usr/bin/qcc)
set(CMAKE_CXX_COMPILER $QNX_HOST/usr/bin/q++)
set(CMAKE_ASM_COMPILER $QNX_HOST/usr/bin/qcc)
set(CMAKE_C_FLAGS "-Vgcc_ntoaarch64le")
set(CMAKE_CXX_FLAGS "-Vgcc_ntoaarch64le")
set(CMAKE_FIND_ROOT_PATH $QNX_TARGET)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
EOF

cd "$CJSON_BUILD_DIR"
cmake "$CJSON_DIR" \
    -DCMAKE_TOOLCHAIN_FILE="$CJSON_BUILD_DIR/qnx-toolchain.cmake" \
    -DCMAKE_INSTALL_PREFIX="$CJSON_INSTALL_DIR" \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_SHARED_LIBS=OFF \
    -DBUILD_TESTING=OFF \
    -DENABLE_CJSON_TEST=OFF

cmake --build . -j$(nproc)
cmake --install .

echo ""
echo "cJSON built and installed to: $CJSON_INSTALL_DIR"
echo ""

# ==========================================
# Step 2: Prepare getrandom() stub for QNX
# ==========================================
echo "--- Preparing getrandom() stub for QNX ---"

# QNX does not provide getrandom(). We provide a stub that reads from /dev/urandom.

# Reset patches from any previous run
cd "$SOURCE_DIR"
git checkout -- libcommon/CMakeLists.txt 2>/dev/null || true
rm -f include/sys/random.h libcommon/getrandom_qnx.c 2>/dev/null || true

# Create sys/random.h header (needed by random_common.c when HAVE_GETRANDOM is defined)
mkdir -p "$SOURCE_DIR/include/sys"
cat > "$SOURCE_DIR/include/sys/random.h" << 'HEADER_EOF'
#ifndef _SYS_RANDOM_H
#define _SYS_RANDOM_H

#include <sys/types.h>

ssize_t getrandom(void *buf, size_t buflen, unsigned int flags);

#endif
HEADER_EOF

# Create getrandom_qnx.c implementation
cat > "$SOURCE_DIR/libcommon/getrandom_qnx.c" << 'STUB_EOF'
#include <sys/random.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

ssize_t getrandom(void *buf, size_t buflen, unsigned int flags)
{
    static int fd = -1;
    size_t total = 0;
    ssize_t n;

    (void)flags;

    if (fd < 0) {
        fd = open("/dev/urandom", O_RDONLY | O_CLOEXEC);
        if (fd < 0) {
            fd = open("/dev/random", O_RDONLY | O_CLOEXEC);
            if (fd < 0) {
                errno = EIO;
                return -1;
            }
        }
    }

    while (total < buflen) {
        n = read(fd, (char *)buf + total, buflen - total);
        if (n <= 0) {
            if (n == 0) errno = EIO;
            return -1;
        }
        total += n;
    }
    return (ssize_t)total;
}
STUB_EOF

# Patch libcommon/CMakeLists.txt:
#  1. Add getrandom_qnx.c to the source list
#  2. Replace the getrandom() check with unconditional HAVE_GETRANDOM

LIBCOMMON_CMAKE="$SOURCE_DIR/libcommon/CMakeLists.txt"

python3 << PYEOF

libcommon_cmake = "$LIBCOMMON_CMAKE"

with open(libcommon_cmake, 'r') as f:
    content = f.read()

# 1. Add getrandom_qnx.c after random_common.c in C_SRC
if 'random_common.c' in content:
    content = content.replace(
        'random_common.c',
        'random_common.c\n\tgetrandom_qnx.c',
        1  # only first occurrence
    )
    print("  -> Added getrandom_qnx.c to C_SRC")
else:
    print("  WARNING: random_common.c not found in C_SRC")

# 2. Replace the elseif(NOT WIN32) block (the getrandom check) with else() + HAVE_GETRANDOM
old_block = """elseif(NOT WIN32)
\tinclude(CheckSymbolExists)
\tcheck_symbol_exists(getrandom \"sys/random.h\" GETRANDOM_FOUND)
\tif(GETRANDOM_FOUND)
\t\tadd_definitions(\"-DHAVE_GETRANDOM\")
\telse()
\t\tmessage(FATAL_ERROR \"C library does not provide getrandom(); enable WITH_TLS instead\")
\tendif()"""

new_block = """else()
\tadd_definitions(\"-DHAVE_GETRANDOM\")"""

if old_block in content:
    content = content.replace(old_block, new_block, 1)
    print("  -> Replaced getrandom check with HAVE_GETRANDOM")
else:
    print("  WARNING: Could not find getrandom check block in CMakeLists.txt")
    print("  Current content around WITH_TLS:")
    for i, line in enumerate(content.split('\\n')):
        if 'WITH_TLS' in line or 'CheckSymbol' in line or 'getrandom' in line or 'GETRANDOM' in line:
            print(f"    {i}: {line}")

with open(libcommon_cmake, 'w') as f:
    f.write(content)

print("Done patching CMakeLists.txt")
PYEOF

echo ""

# ==========================================
# Step 3: Build mosquitto
# ==========================================
echo "--- Building mosquitto ---"
MOSQ_BUILD_DIR="$BUILD_DIR/mosquitto"
mkdir -p "$MOSQ_BUILD_DIR"

cat > "$MOSQ_BUILD_DIR/qnx-toolchain.cmake" << EOF
set(CMAKE_SYSTEM_NAME QNX)
set(CMAKE_SYSTEM_PROCESSOR aarch64)
set(CMAKE_C_COMPILER $QNX_HOST/usr/bin/qcc)
set(CMAKE_CXX_COMPILER $QNX_HOST/usr/bin/q++)
set(CMAKE_ASM_COMPILER $QNX_HOST/usr/bin/qcc)
set(CMAKE_C_FLAGS "-Vgcc_ntoaarch64le -I$SOURCE_DIR/include")
set(CMAKE_CXX_FLAGS "-Vgcc_ntoaarch64le -I$SOURCE_DIR/include")
set(CMAKE_FIND_ROOT_PATH $QNX_TARGET)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
EOF

cd "$MOSQ_BUILD_DIR"
cmake "$SOURCE_DIR" \
    -DCMAKE_TOOLCHAIN_FILE="$MOSQ_BUILD_DIR/qnx-toolchain.cmake" \
    -DCMAKE_INSTALL_PREFIX="$INSTALL_DIR" \
    -DCJSON_INCLUDE_DIR="$CJSON_INSTALL_DIR/include" \
    -DCJSON_LIBRARY="$CJSON_INSTALL_DIR/lib/libcjson.a" \
    -DCMAKE_BUILD_TYPE=Release \
    -DWITH_STATIC_LIBRARIES=ON \
    -DWITH_SHARED_LIBRARIES=ON \
    -DWITH_CLIENTS=OFF \
    -DWITH_SERVER=OFF \
    -DWITH_APPS=OFF \
    -DWITH_LIBS=ON \
    -DWITH_BROKER=OFF \
    -DWITH_PLUGINS=OFF \
    -DWITH_LIB_CPP=OFF \
    -DWITH_TLS=OFF \
    -DWITH_TLS_PSK=OFF \
    -DWITH_WEBSOCKETS=OFF \
    -DWITH_SOCKS=OFF \
    -DWITH_DOCS=OFF \
    -DWITH_TESTS=OFF

cmake --build . -j$(nproc)
cmake --install .

echo ""
echo "=========================================="
echo "Mosquitto cross-compiled successfully!"
echo "=========================================="
echo "Install directory: $INSTALL_DIR"
echo ""
echo "Libraries:"
ls -lh "$INSTALL_DIR/lib/"*mosquitto* 2>/dev/null || echo "  (no libraries found)"
echo ""
echo "Headers:"
ls -lh "$INSTALL_DIR/include/mosquitto.h" 2>/dev/null || echo "  (no headers found)"
echo ""
echo "cJSON dependency:"
ls -lh "$CJSON_INSTALL_DIR/lib/"*cjson* 2>/dev/null || echo "  (no cJSON found)"
echo ""
echo "To use the library:"
echo "  -I$INSTALL_DIR/include -I$CJSON_INSTALL_DIR/include"
echo "  -L$INSTALL_DIR/lib -L$CJSON_INSTALL_DIR/lib"
echo "  -lmosquitto -lcjson"
echo ""
