#!/usr/bin/env bash
###############################################################################
#
# build.sh
#
#   Cross-compiles the CommonAPI + SOME/IP runtime libraries for QNX and
#   produces all libraries, headers, and toolchain in ${OUTPUT_DIR}.
#
#   This script builds ONLY the libraries and unzips the generators:
#     Boost, vsomeip, CommonAPI Core Runtime, CommonAPI SOME/IP Runtime.
#
#   The server and client apps generate their own code and build separately
#   from their own directories — see README.md for commands.
#
#   USAGE
#     bash scripts/build.sh
#
###############################################################################

set -euo pipefail

# ── source the QNX environment (all macros come from here) ─────────────────
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck disable=SC1091
source "${SCRIPT_DIR}/env.sh"

# ── directory shortcuts ────────────────────────────────────────────────────
ROOT_DIR="${ROOT_DIR}"
THIRD_PARTY="${THIRD_PARTY_DIR}"
mkdir -p "${OUTPUT_DIR}"
BUILD="$(cd "${OUTPUT_DIR}" && pwd)"
INTERFACE="${INTERFACE_DIR}"

# ── source paths derived from version macros ──────────────────────────────
BOOST_SRC="${THIRD_PARTY}/${BOOST_DIRNAME}"
VSOMEIP_SRC="${THIRD_PARTY}/vsomeip"
CORE_RT_SRC="${THIRD_PARTY}/capicxx-core-runtime"
SOMEIP_RT_SRC="${THIRD_PARTY}/capicxx-someip-runtime"

CORE_GEN_ZIP="${THIRD_PARTY}/commonapi_core_generator.zip"
SOMEIP_GEN_ZIP="${THIRD_PARTY}/commonapi_someip_generator.zip"

# ── create output directories ──────────────────────────────────────────────
mkdir -p "${BUILD}/lib" "${BUILD}/include" "${BUILD}/generators"

# ── clean stale CMake caches so toolchain changes take effect ──────────────
rm -rf "${BUILD}/cmake-build"

echo
echo "============================================================"
echo "  CommonAPI / vsomeip  QNX Library Cross-Build"
echo "============================================================"
echo "  Target profile : ${QNX_TARGET_PROFILE}"
echo "  Arch           : ${QNX_ARCH}  (qcc: ${QNX_QCC_VARIANT})"
echo "  vsomeip        : ${VSOMEIP_VERSION}"
echo "  core-runtime   : ${COMMONAPI_CORE_RUNTIME_VERSION}"
echo "  someip-runtime : ${COMMONAPI_SOMEIP_RUNTIME_VERSION}"
echo "  generators     : ${COMMONAPI_GENERATOR_VERSION}"
echo "  boost          : ${BOOST_VERSION}"
echo "  output dir     : ${BUILD}"
echo "  build type     : ${BUILD_TYPE}   jobs: ${JOBS}"
echo "============================================================"
echo


###############################################################################
# Step 0 — Generate the CMake toolchain file + compat headers
###############################################################################

COMPAT_INCLUDE="${BUILD}/compat-include"
mkdir -p "${COMPAT_INCLUDE}/sys"

cat > "${COMPAT_INCLUDE}/sys/sockmsg.h" <<'EOF'
#ifndef _SYS_SOCKMSG_H
#define _SYS_SOCKMSG_H
#include <sys/iomsg.h>
#include <sys/neutrino.h>
#ifndef _NTO_COF_INSECURE
#define _NTO_COF_INSECURE 0
#endif
#endif
EOF

cat > "${COMPAT_INCLUDE}/sys/eventfd.h" <<'EOF'
#ifndef _SYS_EVENTFD_H
#define _SYS_EVENTFD_H
#include <fcntl.h>
#ifndef EFD_SEMAPHORE
#define EFD_SEMAPHORE (1 << 0)
#endif
#ifndef EFD_NONBLOCK
#define EFD_NONBLOCK O_NONBLOCK
#endif
#ifndef EFD_CLOEXEC
#define EFD_CLOEXEC O_CLOEXEC
#endif
static inline int eventfd(unsigned int initval, int flags)
{
    (void)initval; (void)flags;
    return -1;
}
#endif
EOF

cat > "${BUILD}/toolchain.cmake" <<TCMEOF
set(CMAKE_SYSTEM_NAME QNX)
set(CMAKE_SYSTEM_PROCESSOR ${QNX_ARCH})
set(CMAKE_C_COMPILER qcc)
set(CMAKE_CXX_COMPILER q++)
set(CMAKE_C_FLAGS_INIT   "-V${QNX_QCC_VARIANT} -D_QNX_SOURCE -DSA_RESTART=0x0040 -DBYTE_ORDER=1234 -DLITTLE_ENDIAN=1234 -DBIG_ENDIAN=4321 -D__BYTE_ORDER=1234 -D__LITTLE_ENDIAN=1234 -D__BIG_ENDIAN=4321 -I${COMPAT_INCLUDE}")
set(CMAKE_CXX_FLAGS_INIT "-V${QNX_QCC_VARIANT} -D_QNX_SOURCE -DSA_RESTART=0x0040 -DBYTE_ORDER=1234 -DLITTLE_ENDIAN=1234 -DBIG_ENDIAN=4321 -D__BYTE_ORDER=1234 -D__LITTLE_ENDIAN=1234 -D__BIG_ENDIAN=4321 -I${COMPAT_INCLUDE}")
set(CMAKE_CXX_FLAGS_RELEASE_INIT "-O3 -DNDEBUG -Wno-narrowing")
set(CMAKE_C_FLAGS_RELEASE_INIT   "-O3 -DNDEBUG")
set(CMAKE_FIND_ROOT_PATH \$ENV{QNX_TARGET}/${QNX_CPUVARDIR})
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY BOTH)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE BOTH)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE BOTH)
TCMEOF

TOOLCHAIN_FILE="${BUILD}/toolchain.cmake"

CMAKE_COMMON_ARGS=(
    -DCMAKE_TOOLCHAIN_FILE="${TOOLCHAIN_FILE}"
    -DCMAKE_INSTALL_PREFIX="${BUILD}"
    -DCMAKE_BUILD_TYPE="${BUILD_TYPE}"
    -DBOOST_ROOT="${BUILD}"
    -DBOOST_INCLUDEDIR="${BUILD}/include"
    -DBOOST_LIBRARYDIR="${BUILD}/lib"
    -DBoost_INCLUDE_DIR="${BUILD}/include"
    -DBoost_USE_MULTITHREADED=ON
    -DBoost_NO_BOOST_CMAKE=ON
    -DBoost_NO_SYSTEM_PATHS=ON
    -DCMAKE_PREFIX_PATH="${BUILD}"
)


###############################################################################
# Step 1 — Build Boost ${BOOST_VERSION}
###############################################################################
echo "============================================================"
echo "  [1/5]  Boost ${BOOST_VERSION}"
echo "============================================================"

JAM_CONFIG="${BUILD}/user-config.jam"
cat > "${JAM_CONFIG}" <<JEOF
using gcc : qnx : ${QNX_GXX_NAME} ;
JEOF

(
    cd "${BOOST_SRC}"
    [ -f b2 ] || ./bootstrap.sh --with-libraries=system,thread,filesystem,log
    ./b2 \
        --user-config="${JAM_CONFIG}" \
        toolset=gcc-qnx \
        target-os=qnx \
        --prefix="${BUILD}" \
        --layout=system \
        link=shared runtime-link=shared threading=multi \
        --with-system --with-thread --with-filesystem --with-log \
        install
)

echo "  -> Boost ${BOOST_VERSION} installed to ${BUILD}"


###############################################################################
# Step 2 — Unzip the CommonAPI code generators (NOT run here)
###############################################################################
echo
echo "============================================================"
echo "  [2/5]  Code Generators ${COMMONAPI_GENERATOR_VERSION} (unzip only)"
echo "============================================================"

CORE_GEN_DIR="${BUILD}/generators/core"
SOMEIP_GEN_DIR="${BUILD}/generators/someip"

mkdir -p "${CORE_GEN_DIR}" "${SOMEIP_GEN_DIR}"
unzip -o -q "${CORE_GEN_ZIP}"   -d "${CORE_GEN_DIR}"
unzip -o -q "${SOMEIP_GEN_ZIP}" -d "${SOMEIP_GEN_DIR}"

CORE_GEN_EXE="$(find "${CORE_GEN_DIR}" -name 'commonapi-core-generator-linux-x86_64' | head -1)"
SOMEIP_GEN_EXE="$(find "${SOMEIP_GEN_DIR}" -name 'commonapi-someip-generator-linux-x86_64' | head -1)"

if [ -z "${CORE_GEN_EXE}" ] || [ -z "${SOMEIP_GEN_EXE}" ]; then
    echo "ERROR: generator executable not found after unzip."
    exit 1
fi
chmod +x "${CORE_GEN_EXE}" "${SOMEIP_GEN_EXE}"
echo "  -> Core generator:    ${CORE_GEN_EXE}"
echo "  -> SOME/IP generator: ${SOMEIP_GEN_EXE}"


###############################################################################
# Step 3 — Build vsomeip ${VSOMEIP_VERSION}
###############################################################################
echo
echo "============================================================"
echo "  [3/5]  vsomeip ${VSOMEIP_VERSION}"
echo "============================================================"

(
    cd "${VSOMEIP_SRC}"
    _actual="$(git describe --tags 2>/dev/null || echo 'unknown')"
    if [ "${_actual}" != "${VSOMEIP_VERSION}" ]; then
        echo "  WARNING: vsomeip is at '${_actual}', expected '${VSOMEIP_VERSION}'"
    fi
)

VSOMEIP_BUILD="${BUILD}/cmake-build/vsomeip"
mkdir -p "${VSOMEIP_BUILD}"
cmake "${CMAKE_COMMON_ARGS[@]}" \
      -DENABLE_SIGNAL_HANDLING=1 \
      -DVSOMEIP_ENABLE_MULTIPLE_ROUTING_MANAGERS=1 \
      -B "${VSOMEIP_BUILD}" \
      -S "${VSOMEIP_SRC}"
cmake --build "${VSOMEIP_BUILD}" -j "${JOBS}"
cmake --install "${VSOMEIP_BUILD}"

echo "  -> vsomeip ${VSOMEIP_VERSION} installed to ${BUILD}"


###############################################################################
# Step 4 — Build CommonAPI Core Runtime ${COMMONAPI_CORE_RUNTIME_VERSION}
###############################################################################
echo
echo "============================================================"
echo "  [4/5]  CommonAPI Core Runtime ${COMMONAPI_CORE_RUNTIME_VERSION}"
echo "============================================================"

(
    cd "${CORE_RT_SRC}"
    _actual="$(git describe --tags 2>/dev/null || echo 'unknown')"
    if [ "${_actual}" != "${COMMONAPI_CORE_RUNTIME_VERSION}" ]; then
        echo "  WARNING: core-runtime is at '${_actual}', expected '${COMMONAPI_CORE_RUNTIME_VERSION}'"
    fi
)

CORE_RT_BUILD="${BUILD}/cmake-build/core-runtime"
mkdir -p "${CORE_RT_BUILD}"
cmake "${CMAKE_COMMON_ARGS[@]}" \
      -B "${CORE_RT_BUILD}" \
      -S "${CORE_RT_SRC}"
cmake --build "${CORE_RT_BUILD}" -j "${JOBS}"
cmake --install "${CORE_RT_BUILD}"

echo "  -> CommonAPI Core Runtime ${COMMONAPI_CORE_RUNTIME_VERSION} installed to ${BUILD}"


###############################################################################
# Step 5 — Build CommonAPI SOME/IP Runtime ${COMMONAPI_SOMEIP_RUNTIME_VERSION}
###############################################################################
echo
echo "============================================================"
echo "  [5/5]  CommonAPI SOME/IP Runtime ${COMMONAPI_SOMEIP_RUNTIME_VERSION}"
echo "============================================================"

(
    cd "${SOMEIP_RT_SRC}"
    _actual="$(git describe --tags 2>/dev/null || echo 'unknown')"
    if [ "${_actual}" != "${COMMONAPI_SOMEIP_RUNTIME_VERSION}" ]; then
        echo "  WARNING: someip-runtime is at '${_actual}', expected '${COMMONAPI_SOMEIP_RUNTIME_VERSION}'"
    fi
)

SOMEIP_RT_BUILD="${BUILD}/cmake-build/someip-runtime"
mkdir -p "${SOMEIP_RT_BUILD}"
cmake "${CMAKE_COMMON_ARGS[@]}" \
      -DCommonAPI_DIR="${BUILD}/lib/cmake/CommonAPI-${COMMONAPI_CORE_RUNTIME_VERSION}" \
      -Dvsomeip3_DIR="${BUILD}/lib/cmake/vsomeip3" \
      -B "${SOMEIP_RT_BUILD}" \
      -S "${SOMEIP_RT_SRC}"
cmake --build "${SOMEIP_RT_BUILD}" -j "${JOBS}"
cmake --install "${SOMEIP_RT_BUILD}"

echo "  -> CommonAPI SOME/IP Runtime ${COMMONAPI_SOMEIP_RUNTIME_VERSION} installed to ${BUILD}"


###############################################################################
# Summary
###############################################################################
echo
echo "============================================================"
echo "  LIBRARY BUILD COMPLETE"
echo "============================================================"
echo
echo "  Output directory: ${BUILD}"
echo
echo "  Libraries:"
find "${BUILD}/lib" -maxdepth 1 -name '*.so' -printf '    %f\n' 2>/dev/null | sort || true
echo
echo "  Toolchain:    ${BUILD}/toolchain.cmake"
echo "  Generators:   ${BUILD}/generators/"
echo
echo "  To build the server app:"
echo "    source scripts/env.sh"
echo "    cd server && cmake -B build -S . && cmake --build build"
echo
echo "  To build the client app:"
echo "    source scripts/env.sh"
echo "    cd client && cmake -B build -S . && cmake --build build"
echo
