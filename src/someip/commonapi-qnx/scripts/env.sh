#!/usr/bin/env bash
###############################################################################
#
# env.sh  —  QNX cross-compilation environment & project configuration
#
#   This script MUST be sourced (not executed):
#
#       source scripts/env.sh
#
#   It exports ALL configuration as environment variables so that
#   download.sh and build.sh can reference them.
#
#   Override any macro BEFORE sourcing this script:
#
#       export QNX_SDP_PATH=/opt/qnx/sdp
#       export QNX_TARGET_PROFILE=native      # rpi | native
#       export OUTPUT_DIR=/tmp/mybuild
#       source scripts/env.sh
#
###############################################################################

# ---------------------------------------------------------------------------
# Resolve the project root (directory above scripts/)
# ---------------------------------------------------------------------------
_ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
export ROOT_DIR="${_ROOT_DIR}"

# ===========================================================================
# 1. COMPONENT VERSIONS
# ===========================================================================
#    Change these to build a different release of each component.
#    download.sh uses them for git clone --branch and release URLs.
#    build.sh  uses them for version verification and directory paths.
# ---------------------------------------------------------------------------
export VSOMEIP_VERSION="3.5.5"
export COMMONAPI_CORE_RUNTIME_VERSION="3.2.4"
export COMMONAPI_SOMEIP_RUNTIME_VERSION="3.2.4"
export COMMONAPI_GENERATOR_VERSION="3.2.15"
export BOOST_VERSION="1.84.0"

# Derived: boost directory name (boost_1_84_0 from 1.84.0)
_boost_underscore="$(echo "${BOOST_VERSION}" | tr '.' '_')"
export BOOST_DIRNAME="boost_${_boost_underscore}"

# ===========================================================================
# 2. TARGET PROFILE  (rpi  or  native)
# ===========================================================================
#    QNX_TARGET_PROFILE selects the target architecture and the qcc variant.
#
#      rpi     → aarch64le  → gcc_ntoaarch64le   (Raspberry Pi 4 / ARM64)
#      native  → x86_64     → gcc_ntox86_64_gpp   (QNX x86_64 native)
#
#    You can also set QNX_ARCH directly to override:
#       export QNX_ARCH=aarch64le
# ---------------------------------------------------------------------------
export QNX_TARGET_PROFILE="${QNX_TARGET_PROFILE:-rpi}"

# If QNX_ARCH is not explicitly set, derive it from the profile
if [ -z "${QNX_ARCH:-}" ]; then
    case "${QNX_TARGET_PROFILE}" in
        rpi)    export QNX_ARCH="aarch64le" ;;
        native) export QNX_ARCH="x86_64" ;;
        *)
            echo "ERROR: QNX_TARGET_PROFILE='${QNX_TARGET_PROFILE}' unknown."
            echo "       Use 'rpi' or 'native'."
            return 1 2>/dev/null || exit 1
            ;;
    esac
fi

# Map QNX_ARCH → qcc variant, CPU var dir, and cross-compiler name
case "${QNX_ARCH}" in
    aarch64le)
        export QNX_CPUVARDIR="aarch64le"
        export QNX_QCC_VARIANT="gcc_ntoaarch64le"
        export QNX_GXX_NAME="ntoaarch64-g++"
        ;;
    x86_64)
        export QNX_CPUVARDIR="x86_64"
        export QNX_QCC_VARIANT="gcc_ntox86_64_gpp"
        export QNX_GXX_NAME="ntox86_64-g++"
        ;;
    *)
        echo "ERROR: Unsupported QNX_ARCH='${QNX_ARCH}'. Use aarch64le or x86_64."
        return 1 2>/dev/null || exit 1
        ;;
esac

# ===========================================================================
# 3. QNX SDP LOCATION
# ===========================================================================
#    The user may set QNX_SDP_PATH explicitly.  Otherwise we search a list of
#    common locations for the qnxsdp-env.sh script.
# ---------------------------------------------------------------------------
if [ -z "${QNX_SDP_PATH:-}" ]; then
    _CANDIDATES=(
        "${HOME}/qnx/sdp"
        "${HOME}/QNX_SDP"
        "/opt/qnx"
        "/opt/qnx/sdp"
        "/opt/qnx/software_platform"
    )
    for _C in "${_CANDIDATES[@]}"; do
        if [ -f "${_C}/qnxsdp-env.sh" ]; then
            QNX_SDP_PATH="${_C}"
            break
        fi
    done
fi

if [ -z "${QNX_SDP_PATH:-}" ] || [ ! -f "${QNX_SDP_PATH}/qnxsdp-env.sh" ]; then
    echo "==================================================================="
    echo " ERROR: Could not locate the QNX SDP."
    echo ""
    echo " Set QNX_SDP_PATH to the directory that contains qnxsdp-env.sh"
    echo " and source this script again, e.g.:"
    echo ""
    echo "     export QNX_SDP_PATH=/path/to/qnx/sdp"
    echo "     source scripts/env.sh"
    echo "==================================================================="
    return 1 2>/dev/null || exit 1
fi

export QNX_SDP_PATH

# Source the QNX SDP environment (sets QNX_HOST, QNX_TARGET, PATH, …)
# shellcheck disable=SC1090
source "${QNX_SDP_PATH}/qnxsdp-env.sh"

# ===========================================================================
# 4. OUTPUT & DIRECTORY LAYOUT
# ===========================================================================
#    OUTPUT_DIR is the top-level build output directory.
#    Override with:  export OUTPUT_DIR=/tmp/mybuild
#
#    LIBS_DIR points to the commonapi-qnx project root (where build-rpi/ lives).
#    The server/client CMakeLists read this to find the libraries + toolchain.
#
#    Both are forced to ABSOLUTE paths so they survive directory changes
#    (the server/client are built from a different directory).
# ---------------------------------------------------------------------------
export LIBS_DIR="${ROOT_DIR}"

# Resolve OUTPUT_DIR: user override or default build-<profile>
if [ -z "${OUTPUT_DIR:-}" ]; then
    OUTPUT_DIR="${ROOT_DIR}/build-${QNX_TARGET_PROFILE}"
fi
# Make it absolute (resolve relative paths like "build-rpi")
mkdir -p "${OUTPUT_DIR}"
export OUTPUT_DIR="$(cd "${OUTPUT_DIR}" && pwd)"

export THIRD_PARTY_DIR="${ROOT_DIR}/third_party"
export INTERFACE_DIR="${ROOT_DIR}/interface"

# ===========================================================================
# 5. BUILD CONFIG
# ===========================================================================
export BUILD_TYPE="${BUILD_TYPE:-Release}"
export JOBS="${JOBS:-$(nproc 2>/dev/null || echo 4)}"

# ===========================================================================
# Summary
# ===========================================================================
echo "-----------------------------------------------------------------------"
echo " CommonAPI-QNX environment"
echo "-----------------------------------------------------------------------"
echo "  Versions:"
echo "    vsomeip                     : ${VSOMEIP_VERSION}"
echo "    capicxx-core-runtime        : ${COMMONAPI_CORE_RUNTIME_VERSION}"
echo "    capicxx-someip-runtime      : ${COMMONAPI_SOMEIP_RUNTIME_VERSION}"
echo "    CommonAPI generators        : ${COMMONAPI_GENERATOR_VERSION}"
echo "    Boost                       : ${BOOST_VERSION}"
echo "  Target:"
echo "    QNX_TARGET_PROFILE          : ${QNX_TARGET_PROFILE}"
echo "    QNX_ARCH                    : ${QNX_ARCH}"
echo "    QNX_QCC_VARIANT             : ${QNX_QCC_VARIANT}"
echo "    QNX_GXX_NAME                : ${QNX_GXX_NAME}"
echo "  Paths:"
echo "    QNX_SDP_PATH                : ${QNX_SDP_PATH}"
echo "    QNX_HOST                    : ${QNX_HOST}"
echo "    QNX_TARGET                  : ${QNX_TARGET}"
echo "    ROOT_DIR                    : ${ROOT_DIR}"
echo "    LIBS_DIR                    : ${LIBS_DIR}"
echo "    OUTPUT_DIR                  : ${OUTPUT_DIR}"
echo "    THIRD_PARTY_DIR             : ${THIRD_PARTY_DIR}"
echo "    BUILD_TYPE / JOBS           : ${BUILD_TYPE} / ${JOBS}"
echo "-----------------------------------------------------------------------"
