#!/bin/sh
# Download BCM43455 WiFi firmware for Raspberry Pi 5
# Source: pyavitz/firmware (GitHub mirror of linux-firmware, Pi 5 board-specific files)
set -e

DIR="$(cd "$(dirname "$0")" && pwd)"
mkdir -p "$DIR/firmware"

BASE_URL="https://raw.githubusercontent.com/pyavitz/firmware/main"

curl -fL -o "$DIR/firmware/brcmfmac43455-sdio.raspberrypi,5-model-b.bin" \
    "$BASE_URL/brcmfmac43455-sdio.raspberrypi,5-model-b.bin"

curl -fL -o "$DIR/firmware/brcmfmac43455-sdio.raspberrypi,5-model-b.clm_blob" \
    "$BASE_URL/brcmfmac43455-sdio.raspberrypi,5-model-b.clm_blob"

curl -fL -o "$DIR/firmware/brcmfmac43455-sdio.raspberrypi,5-model-b.txt" \
    "$BASE_URL/brcmfmac43455-sdio.raspberrypi,5-model-b.txt"

echo "Firmware downloaded to $DIR/firmware/"
ls -la "$DIR/firmware/"
