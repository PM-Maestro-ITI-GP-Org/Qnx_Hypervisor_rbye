#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
WORK_DIR="${QT6_WORK_DIR:-$(dirname "$SCRIPT_DIR")}"
CONFIG_FILE="${QT6_CONFIG:-${WORK_DIR}/.build-config}"
QT6_SOURCE_DIR="${QT6_SOURCE_DIR:-${WORK_DIR}/qt6-source}"

# ─── Module metadata catalog ─────────────────────────────────────
declare -A CATALOG
CATALOG["qtdeclarative"]="UI:QML + Quick + Controls2"
CATALOG["qtshadertools"]="UI:Shader compiler"
CATALOG["qtsvg"]="UI:SVG rendering"
CATALOG["qtquick3d"]="UI:3D in Qt Quick"
CATALOG["qtquick3dphysics"]="UI:3D physics"
CATALOG["qtquickeffectmaker"]="UI:Visual effects"
CATALOG["qtquicktimeline"]="UI:Timeline anim"
CATALOG["qt3d"]="UI:Qt3D framework"
CATALOG["qtimageformats"]="Image:TIFF/WebP codecs"
CATALOG["qtmultimedia"]="Media:Audio/video"
CATALOG["qtcharts"]="Data:Chart widgets"
CATALOG["qtdatavis3d"]="Data:3D viz"
CATALOG["qtgraphs"]="Data:Graphs"
CATALOG["qtlocation"]="Data:Maps"
CATALOG["qtpositioning"]="Data:GPS"
CATALOG["qtwebsockets"]="Net:WebSocket"
CATALOG["qtcoap"]="Net:CoAP"
CATALOG["qtmqtt"]="Net:MQTT"
CATALOG["qtconnectivity"]="Net:Bluetooth/NFC"
CATALOG["qtgrpc"]="Net:gRPC"
CATALOG["qthttpserver"]="Net:HTTP server"
CATALOG["qtnetworkauth"]="Net:OAuth"
CATALOG["qtserialbus"]="I/O:CAN/Modbus"
CATALOG["qtserialport"]="I/O:Serial ports"
CATALOG["qtsensors"]="I/O:Sensors"
CATALOG["qt5compat"]="Utils:Qt5 compat"
CATALOG["qtscxml"]="Utils:State machine"
CATALOG["qtremoteobjects"]="Utils:IPC"
CATALOG["qtlottie"]="Utils:Lottie anim"
CATALOG["qtspeech"]="Utils:TTS"
CATALOG["qtopcua"]="Proto:OPC UA"
CATALOG["qtlanguageserver"]="Proto:LSP"
CATALOG["qttools"]="Dev:Designer/Linguist"
CATALOG["qttranslations"]="Dev:Translations"
CATALOG["qtdoc"]="Dev:Documentation"
CATALOG["qtvirtualkeyboard"]="Input:Virtual KB"
CATALOG["qtactiveqt"]="Win:ActiveX/COM"
CATALOG["qtwayland"]="Lin:Wayland"
CATALOG["qtwebengine"]="Lin:Chromium"
CATALOG["qtwebview"]="Lin:WebView"
CATALOG["qtwebchannel"]="Lin:WebChannel"

BROKEN_ON_QNX="qtactiveqt qtwayland qtwebengine qtwebview qtwebchannel qtgraphs qt3d qtdatavis3d qtquick3d qtquick3dphysics qtquickeffectmaker"

WHITE='\033[0;37m'; BLUE='\033[0;34m'; NC='\033[0m'

# ─── Presets (used by --preset flag) ──────────────────────────────

get_preset_modules() {
    local preset="$1"
    local available; available=$(detect_available)
    local selected=()
    case "$preset" in
        minimal)
            contains "qtdeclarative" $available && selected+=("qtdeclarative")
            contains "qtshadertools" $available && selected+=("qtshadertools")
            ;;
        default)
            for m in qtdeclarative qtshadertools qtsvg qtimageformats qtmultimedia; do
                contains "$m" $available && selected+=("$m")
            done
            ;;
        ivi)
            for m in qtdeclarative qtshadertools qtsvg qtimageformats qtmultimedia qtquick3d qtwebsockets qtremoteobjects qtcharts qtmqtt qtlocation qtpositioning qtserialport qtserialbus; do
                contains "$m" $available && selected+=("$m")
            done
            ;;
        full)
            selected=($available)
            ;;
    esac
    echo "${selected[@]}"
}

# ─── Helpers ─────────────────────────────────────────────────────

detect_available() {
    for dir in "${QT6_SOURCE_DIR}"/qt*; do
        local name; name=$(basename "$dir")
        [ "$name" = "qtbase" ] && continue
        [ -f "$dir/CMakeLists.txt" ] && echo "$name"
    done
}

contains() {
    local needle="$1"; shift
    for item in "$@"; do [ "$item" = "$needle" ] && return 0; done
    return 1
}

get_info() { echo "${CATALOG[$1]:-Other:Unknown module}"; }
get_category() { echo "$1" | cut -d: -f1; }
get_desc()     { echo "$1" | cut -d: -f2; }

is_broken() {
    for b in $BROKEN_ON_QNX; do [ "$b" = "$1" ] && return 0; done
    return 1
}

use_whiptail() { command -v whiptail &>/dev/null; }

# ─── Whiptail module checklist (main UI) ─────────────────────────

whiptail_checklist() {
    # Set blue/white color theme
    export NEWT_COLORS='
root=white,blue
window=white,blue
border=white,blue
title=white,blue
button=black,white
compactbutton=white,blue
listbox=white,blue
actlistbox=black,white
textbox=white,blue
acttextbox=black,white
entry=white,blue
disentry=white,blue
label=white,blue
help=white,blue
'
    local pre_selected=("$@")
    local available; available=$(detect_available)
    local sorted=($available)
    local checklist=()

    for m in "${sorted[@]}"; do
        local desc; desc=$(get_desc "$(get_info "$m")")
        local note=""
        is_broken "$m" && note=" [!]"
        local state="OFF"
        for s in "${pre_selected[@]}"; do [ "$s" = "$m" ] && state="ON" && break; done
        local label="${m}  ${desc}${note}"
        [ ${#label} -gt 80 ] && label="${m}  ${desc:0:50}..${note}"
        checklist+=("$m" "$label" "$state")
    done

    whiptail --title "Qt6 QNX - Module Selection" \
        --backtitle "Qt6 QNX — select additional modules below" \
        --checklist "Select additional Qt modules to build:
   [*] qtbase              Core + Gui + Widgets + Network  [always built]" 36 90 26 \
        "${checklist[@]}" 3>&1 1>&2 2>&3
}

# ─── Text module selection (fallback) ────────────────────────────

text_checklist() {
    local pre_selected=("$@")
    local available; available=$(detect_available)
    local sorted=($available)
    local result=()

    local cats=()
    for m in "${sorted[@]}"; do
        local cat; cat=$(get_category "$(get_info "$m")")
        contains "$cat" "${cats[@]}" || cats+=("$cat")
    done

    >&2 echo ""
    >&2 echo "═══ Module Selection ═══"
    >&2 echo "y=include  n=skip  Enter=default  q=quit"
    >&2 echo ""
    >&2 echo "  qtbase (always built)"

    for cat in "${cats[@]}"; do
        >&2 echo "─── ${cat} ───"
        for m in "${sorted[@]}"; do
            local c; c=$(get_category "$(get_info "$m")")
            [ "$c" != "$cat" ] && continue
            local desc; desc=$(get_desc "$(get_info "$m")")
            local note=""
            is_broken "$m" && note=" [may fail on QNX]"
            local default="n"
            for s in "${pre_selected[@]}"; do [ "$s" = "$m" ] && default="y" && break; done
            read -r -p "  ${m} (${desc})${note} [${default}]: " ans
            [ "$ans" = "q" ] && break 2
            case "${ans:-$default}" in y|Y) result+=("$m") ;; esac
        done
        >&2 echo ""
    done
    echo "${result[@]}"
}

# ─── Main ─────────────────────────────────────────────────────────

AVAILABLE=$(detect_available)
NUM_AVAILABLE=$(echo "$AVAILABLE" | wc -w)

echo ""
echo -e "${BLUE}╔═══════════════════════════════════════════╗${NC}"
echo -e "${BLUE}║   Qt6 QNX Build Configuration            ║${NC}"
echo -e "${BLUE}╚═══════════════════════════════════════════╝${NC}"
echo -e "Source: ${QT6_SOURCE_DIR}"
echo -e "${BLUE}Modules available: ${NUM_AVAILABLE}${NC}"
echo ""

[ "$NUM_AVAILABLE" -eq 0 ] && echo "ERROR: No Qt modules found. Run scripts/download_libs.sh first." && exit 1

# Handle --preset flag
if [ $# -ge 1 ] && [ "$1" = "--preset" ] && [ -n "${2:-}" ]; then
    SELECTED=($(get_preset_modules "$2"))
    PRESET="$2"
else
    PRESET="custom"

    # Load previous config if it exists, otherwise use defaults
    local_default=()
    if [ -f "${CONFIG_FILE}" ]; then
        source "${CONFIG_FILE}"
        for m in ${SELECTED_MODULES:-}; do
            [ "$m" = "qtbase" ] && continue
            contains "$m" $AVAILABLE && local_default+=("$m")
        done
    fi
    if [ ${#local_default[@]} -eq 0 ]; then
        for m in qtdeclarative qtshadertools qtsvg qtimageformats qtmultimedia qtquick3d qtwebsockets qtmqtt; do
            contains "$m" $AVAILABLE && local_default+=("$m")
        done
    fi

    FORCE_TEXT=0
    for arg in "$@"; do [ "$arg" = "--text" ] && FORCE_TEXT=1; done
    [ ! -t 1 ] && FORCE_TEXT=1

    if use_whiptail && [ "$FORCE_TEXT" -eq 0 ]; then
        SELECTED_RAW=$(whiptail_checklist "${local_default[@]}")
        [ $? -ne 0 ] && echo "Cancelled." && exit 1
        eval "SELECTED=($SELECTED_RAW)"
    else
        SELECTED=($(text_checklist "${local_default[@]}"))
    fi
fi

# Always include qtbase
SELECTED=("qtbase" "${SELECTED[@]}")
SELECTED=($(printf "%s\n" "${SELECTED[@]}" | sort -u))

# ─── Save config ─────────────────────────────────────────────────

cat > "${CONFIG_FILE}" << EOF
# Qt6 QNX Build Configuration
# Generated by $(date)
SELECTED_MODULES="${SELECTED[*]}"
EOF

echo ""
echo "✅ Configuration saved to ${CONFIG_FILE}"
echo "Selected modules (${#SELECTED[@]}):"
printf "  %s\n" "${SELECTED[@]}"
echo ""
echo "Run: bash scripts/build.sh"
