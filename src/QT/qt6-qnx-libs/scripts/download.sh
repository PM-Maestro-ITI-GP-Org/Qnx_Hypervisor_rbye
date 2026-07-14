#!/bin/bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
WORK_DIR="${QT6_WORK_DIR:-$(dirname "$SCRIPT_DIR")}"
QT6_SOURCE_DIR="${QT6_SOURCE_DIR:-${WORK_DIR}/qt6-source}"
CONFIG_FILE="${QT6_CONFIG:-${WORK_DIR}/.build-config}"
QT_VERSION="${QT6_VERSION:-6.8.3}"
QT_MAJOR_MINOR="${QT_VERSION%.*}"
ARCHIVE_NAME="qt-everywhere-src-${QT_VERSION}.tar.xz"
ARCHIVE_PATH="${WORK_DIR}/${ARCHIVE_NAME}"

BLUE='\033[0;34m'; RED='\033[0;31m'; NC='\033[0m'
info()    { echo -e "${BLUE}[INFO]${NC} $1"; }
error()   { echo -e "${RED}[ERROR]${NC} $1"; exit 1; }

echo ""
echo "=========================================="
echo " Qt Source Downloader"
echo " Qt ${QT_VERSION}"
echo "=========================================="
echo ""

# Already extracted?
if [ -d "$QT6_SOURCE_DIR" ] && [ -f "${QT6_SOURCE_DIR}/.cmake.conf" ]; then
    info "Qt source already exists at ${QT6_SOURCE_DIR}"
    exit 0
fi

# ─── Download tarball ───────────────────────────────────────────

DEFAULT_URL="https://download.qt.io/archive/qt/${QT_MAJOR_MINOR}/${QT_VERSION}/single/${ARCHIVE_NAME}"
MIRRORS=(
    "https://qtproject.mirror.liquidtelecom.com/archive/qt/${QT_MAJOR_MINOR}/${QT_VERSION}/single/${ARCHIVE_NAME}"
    "https://ftp.fau.de/qtproject/archive/qt/${QT_MAJOR_MINOR}/${QT_VERSION}/single/${ARCHIVE_NAME}"
    "https://mirror.accum.se/mirror/qt.io/qtproject/archive/qt/${QT_MAJOR_MINOR}/${QT_VERSION}/single/${ARCHIVE_NAME}"
)
MIN_SIZE_BYTES=500000000

DOWNLOADED=false
for url in "${DEFAULT_URL}" "${MIRRORS[@]}"; do
    if [ -f "$ARCHIVE_PATH" ]; then
        local_size=$(stat -c%s "$ARCHIVE_PATH" 2>/dev/null || echo 0)
        if [ "$local_size" -ge "$MIN_SIZE_BYTES" ]; then
            info "Archive already exists (size OK)."
            DOWNLOADED=true
            break
        fi
        rm -f "$ARCHIVE_PATH"
    fi
    info "Downloading: ${url}"
    if curl --location --fail --progress-bar --continue-at - --output "$ARCHIVE_PATH" "$url"; then
        echo; DOWNLOADED=true; break
    fi
done
[ "$DOWNLOADED" = false ] && error "Failed to download from all URLs."

# ─── Extract ────────────────────────────────────────────────────

info "Extracting archive..."
rm -rf "$QT6_SOURCE_DIR"
EXTRACT_TMP="${WORK_DIR}/.tmp_extract_$$"
mkdir -p "$EXTRACT_TMP"
tar -xf "$ARCHIVE_PATH" -C "$EXTRACT_TMP"

EXTRACTED_DIR=$(find "$EXTRACT_TMP" -maxdepth 1 -type d -name "qt-everywhere-src-*" | head -1)
[ -z "$EXTRACTED_DIR" ] && error "Could not find extracted directory."

mv "$EXTRACTED_DIR" "$QT6_SOURCE_DIR"
rm -rf "$EXTRACT_TMP"

echo ""
echo "=========================================="
info "Download complete → ${QT6_SOURCE_DIR}"
echo " Next step: bash scripts/build.sh"
echo "=========================================="
