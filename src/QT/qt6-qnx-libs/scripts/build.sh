#!/bin/bash
set -euo pipefail

# Auto-detect base directory (parent of scripts/)
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
WORK_DIR="${QT6_WORK_DIR:-$(dirname "$SCRIPT_DIR")}"

QT_VERSION="${QT6_VERSION:-6.8.3}"
QT6_SOURCE_DIR="${QT6_SOURCE_DIR:-${WORK_DIR}/qt6-source}"
OUTPUT_DIR="${QT6_OUTPUT_DIR:-${WORK_DIR}/build_dir}"
HOST_QT_DIR="${OUTPUT_DIR}/host_qt"
WORKING_DIR="${QT6_WORKING_DIR:-${WORK_DIR}/build}"
HOST_BUILD_DIR="${WORKING_DIR}/host"
QNX_BUILD_DIR="${WORKING_DIR}/qnx"
CONFIGURE_SCRIPT="${QT6_CONFIGURE_SCRIPT:-${WORK_DIR}/scripts/configure.sh}"
CONFIG_FILE="${QT6_CONFIG:-${WORK_DIR}/.build-config}"
TOOLCHAIN_FILE="${QT6_TOOLCHAIN:-${WORK_DIR}/cmake/toolchain_qnx_aarch64le.cmake}"
QNX_SOFTWARE_SH="${QT6_QNX_ENV:-}"
NUM_JOBS="${QT6_JOBS:-$(nproc)}"

BLUE='\033[0;34m'; RED='\033[0;31m'; NC='\033[0m'
print_info()    { echo -e "${BLUE}[INFO]${NC} $1"; }
print_error()   { echo -e "${RED}[ERROR]${NC} $1"; exit 1; }

detect_modules() {
    for dir in "${QT6_SOURCE_DIR}"/qt*; do
        local name; name=$(basename "$dir")
        [ "$name" = "qtbase" ] && continue
        [ -f "$dir/CMakeLists.txt" ] && echo "$name"
    done
}

build_skip_flags() {
    local selected="$1"
    local flags=""
    for mod in $(detect_modules); do
        local skip=1
        for s in $selected; do [ "$mod" = "$s" ] && skip=0 && break; done
        [ "$skip" -eq 1 ] && flags="$flags -DBUILD_${mod}=OFF"
    done
    echo "$flags"
}

pre_copy_helper_files() {
    # $1 = build dir
    # When CMAKE_INSTALL_PREFIX is set, QT_WILL_INSTALL=TRUE and Qt skips copying
    # these cmake helper files during configure (only installed later). Other modules
    # (qtdeclarative, qtquick3d) need them present in the build tree at configure time.
    local build_dir="$1"
    local qt6_cmake_dir="${build_dir}/qtbase/lib/cmake/Qt6"
    mkdir -p "${qt6_cmake_dir}"
    for f in QtTargetHelpers.cmake QtBuildHelpers.cmake; do
        cp "${QT6_SOURCE_DIR}/qtbase/cmake/${f}" "${qt6_cmake_dir}/"
    done
}

load_config() {
    if [ ! -f "${CONFIG_FILE}" ]; then
        print_info "No config found. Running configure.sh..."
        bash "${CONFIGURE_SCRIPT}"
        echo ""
    fi
    source "${CONFIG_FILE}"
    SELECTED_MODULES="${SELECTED_MODULES:-qtbase qtdeclarative qtshadertools}"
}

check_prerequisites() {
    [ -f "${QNX_SOFTWARE_SH}" ]  || print_error "QNX SDK not found: ${QNX_SOFTWARE_SH}"
    [ -d "${QT6_SOURCE_DIR}/qtbase" ] || print_error "Qt source not found at ${QT6_SOURCE_DIR}. Run scripts/download.sh first."
    [ -f "${TOOLCHAIN_FILE}" ]   || print_error "Toolchain not found: ${TOOLCHAIN_FILE}"
}

source_qnx_env() {
    print_info "Sourcing QNX environment..."
    source "${QNX_SOFTWARE_SH}"
}

detect_generator() {
    command -v ninja &>/dev/null && echo "-GNinja" || echo ""
}

step1_build_host_qt() {
    print_info "=== Step 1: Build host Qt ==="

    # Skip if host Qt already built and not forced
    if [ -z "${QT6_FORCE_REBUILD:-}" ] && [ -f "${HOST_QT_DIR}/lib/cmake/Qt6/Qt6Config.cmake" ]; then
        print_info "Host Qt already present at ${HOST_QT_DIR}, skipping rebuild (set QT6_FORCE_REBUILD=1 to force)."
        return 0
    fi

    rm -rf "${HOST_BUILD_DIR}" "${HOST_QT_DIR}"
    mkdir -p "${HOST_BUILD_DIR}"

    local skip_flags; skip_flags=$(build_skip_flags "$SELECTED_MODULES")
    local gen; gen=$(detect_generator)

    # Pre-copy cmake helper files from source to build tree
    pre_copy_helper_files "${HOST_BUILD_DIR}"

    print_info "Configuring host Qt..."
    # shellcheck disable=SC2086
    cmake -S "${QT6_SOURCE_DIR}" -B "${HOST_BUILD_DIR}" \
        ${gen} \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX="${HOST_QT_DIR}" \
        -DQT_BUILD_EXAMPLES=OFF \
        -DQT_BUILD_TESTS=OFF \
        -DINPUT_opengl=no \
        -DINPUT_dbus=no \
        ${skip_flags} \
        2>&1 | tee "${HOST_BUILD_DIR}/configure.log"

    print_info "Building host Qt (${NUM_JOBS} jobs)..."
    cmake --build "${HOST_BUILD_DIR}" --parallel "${NUM_JOBS}" 2>&1 | tee "${HOST_BUILD_DIR}/build.log"

    print_info "Installing host Qt..."
    cmake --install "${HOST_BUILD_DIR}" 2>&1 | tee "${HOST_BUILD_DIR}/install.log"

    print_info "Host tools:"
    ls "${HOST_QT_DIR}/libexec/" 2>/dev/null || ls "${HOST_QT_DIR}/bin/" 2>/dev/null || true
}

step2_build_qnx_qt() {
    print_info "=== Step 2: Cross-compile Qt for QNX ==="

    # Incremental by default: keep build/qnx so ninja skips up-to-date targets and
    # resumes after a failed/interrupted run instead of recompiling ~4600 targets.
    # Only wipe on an explicit force (make rebuild / QT6_FORCE_REBUILD=1).
    if [ -n "${QT6_FORCE_REBUILD:-}" ]; then
        print_info "Force rebuild: wiping ${QNX_BUILD_DIR} and QNX install output."
        rm -rf "${QNX_BUILD_DIR}"
        # Clean previous QNX install output but preserve host_qt (needed for QT_HOST_PATH)
        if [ -d "${OUTPUT_DIR}" ]; then
            for item in "${OUTPUT_DIR}"/*; do
                [ "$(basename "$item")" = "host_qt" ] && continue
                rm -rf "$item"
            done
        fi
    elif [ -f "${QNX_BUILD_DIR}/CMakeCache.txt" ]; then
        print_info "Resuming incremental QNX build in ${QNX_BUILD_DIR} (set QT6_FORCE_REBUILD=1 to force a clean rebuild)."
    fi
    mkdir -p "${QNX_BUILD_DIR}"

    # Pre-copy cmake helper files (same reason as step1)
    pre_copy_helper_files "${QNX_BUILD_DIR}"

    local skip_flags; skip_flags=$(build_skip_flags "$SELECTED_MODULES")
    local gen; gen=$(detect_generator)

    print_info "Configuring Qt for QNX..."
    # shellcheck disable=SC2086
    cmake -S "${QT6_SOURCE_DIR}" -B "${QNX_BUILD_DIR}" \
        ${gen} \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_TOOLCHAIN_FILE="${TOOLCHAIN_FILE}" \
        -DQT_HOST_PATH="${HOST_QT_DIR}" \
        -DCMAKE_STAGING_PREFIX="${OUTPUT_DIR}" \
        -DCMAKE_INSTALL_PREFIX=/qt \
        -DQT_BUILD_EXAMPLES=OFF \
        -DQT_BUILD_TESTS=OFF \
        -DINPUT_opengl=no \
        -DINPUT_dbus=no \
        -DFEATURE_libresolv=OFF \
        ${skip_flags} \
        2>&1 | tee "${QNX_BUILD_DIR}/configure.log"

    print_info "Building Qt for QNX (${NUM_JOBS} jobs)..."
    cmake --build "${QNX_BUILD_DIR}" --parallel "${NUM_JOBS}" 2>&1 | tee "${QNX_BUILD_DIR}/build.log"

    print_info "Installing Qt for QNX..."
    cmake --install "${QNX_BUILD_DIR}" 2>&1 | tee "${QNX_BUILD_DIR}/install.log"

    print_info "Output:"
    ls -la "${OUTPUT_DIR}/lib/"libQt6*.so* 2>/dev/null || true
    ls "${OUTPUT_DIR}/qml/" 2>/dev/null || true
}

show_help() {
    cat << EOF
Usage: bash build.sh

Build Qt ${QT_VERSION} for QNX aarch64le.

Environment variables:
  QT6_QNX_ENV       Path to qnxsdp-env.sh (REQUIRED)
  QT6_WORK_DIR      Project root (default: parent of scripts/)
  QT6_SOURCE_DIR    Qt source directory (default: workdir/qt6-source)
  QT6_QNX_ENV       Path to qnxsdp-env.sh (REQUIRED)
  QT6_OUTPUT_DIR    Final install dir (default: workdir/build_dir)
  QT6_WORKING_DIR   Build scratch base (default: workdir/build)
                     host build -> <working_dir>/host
                     qnx build  -> <working_dir>/qnx
  QT6_TOOLCHAIN     Toolchain file (default: workdir/cmake/toolchain_qnx_aarch64le.cmake)
  QT6_CONFIG        Build config file (default: workdir/.build-config)
  QT6_JOBS          Parallel jobs (default: nproc)

Example:
  export QT6_QNX_ENV=/opt/qnx800/qnxsdp-env.sh
  bash scripts/configure.sh
  bash scripts/download.sh
  bash scripts/build.sh
EOF
}

main() {
    echo -e "${BLUE}==========================================${NC}"
    echo -e "${BLUE}  Qt ${QT_VERSION} for QNX aarch64le${NC}"
    echo -e "${BLUE}==========================================${NC}"
    echo -e " Host Qt:  ${HOST_QT_DIR}"
    echo -e " Output:   ${OUTPUT_DIR}"
    echo -e " Jobs:     ${NUM_JOBS}"
    echo ""

    check_prerequisites
    load_config
    source_qnx_env
    step1_build_host_qt
    step2_build_qnx_qt

    echo -e "${BLUE}==========================================${NC}"
    echo -e "${BLUE}  Build complete!${NC}"
    echo -e "${BLUE}  QNX Qt: ${OUTPUT_DIR}${NC}"
    echo -e "${BLUE}==========================================${NC}"
}

main "$@"
