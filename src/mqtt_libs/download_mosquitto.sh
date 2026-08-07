#!/bin/bash
# Download mosquitto source code

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

MOSQUITTO_VERSION="2.0.20"
SOURCE_DIR="${SCRIPT_DIR}/mosquitto-${MOSQUITTO_VERSION}"
DOWNLOAD_DIR="${SCRIPT_DIR}"

echo "Creating download directory: $DOWNLOAD_DIR"
mkdir -p "$DOWNLOAD_DIR"

if [ -d "$SOURCE_DIR" ]; then
    echo "Mosquitto source already exists at: $SOURCE_DIR"
    read -p "Do you want to download again? (y/N) " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        echo "Using existing source"
        exit 0
    fi
    rm -rf "$SOURCE_DIR"
fi

echo "Downloading mosquitto ${MOSQUITTO_VERSION}..."
cd "$DOWNLOAD_DIR"

if command -v git &> /dev/null; then
    echo "Using git..."
    if git clone --depth 1 https://github.com/eclipse/mosquitto.git mosquitto-${MOSQUITTO_VERSION}; then
        cd mosquitto-${MOSQUITTO_VERSION}
        echo "Checking out master branch..."
        git checkout master 2>/dev/null || git checkout main
        cd ..
        echo "Git clone successful."
    else
        echo "Git failed. Try running: sudo apt install git"
        exit 1
    fi
else
    echo "Error: git not found. Please install git: sudo apt install git"
    exit 1
fi

# Verify CMakeLists.txt exists
if [ ! -f "$SOURCE_DIR/CMakeLists.txt" ]; then
    echo "Error: CMakeLists.txt not found in mosquitto source"
    exit 1
fi

echo ""
echo "Mosquitto source ready at: $SOURCE_DIR"
echo "Next step: ./cross_compile_qnx.sh"
