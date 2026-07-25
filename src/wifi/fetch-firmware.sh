#!/bin/sh
# Download BCM43455 WiFi firmware for Raspberry Pi 5
# Run once before building the hypervisor image.
set -e
DIR="$(cd "$(dirname "$0")" && pwd)"
mkdir -p "$DIR/firmware"
cd "$DIR/firmware"

BRANCH="bookworm"
BASE_BCM="https://raw.githubusercontent.com/RPi-Distro/firmware-nonfree/${BRANCH}/debian/config/brcm80211/brcm"
BASE_CYP="https://raw.githubusercontent.com/RPi-Distro/firmware-nonfree/${BRANCH}/debian/config/brcm80211/cypress"

# The RPi5 uses symlinks from model-specific names to the base firmware files.
# The .bin and .clm_blob come from the cypress directory, the .txt from brcm.
wget -N "$BASE_CYP/cyfmac43455-sdio-standard.bin"    -O brcmfmac43455-sdio.bin
wget -N "$BASE_CYP/cyfmac43455-sdio.clm_blob"         -O brcmfmac43455-sdio.clm_blob
wget -N "$BASE_BCM/brcmfmac43455-sdio.txt"             -O brcmfmac43455-sdio.txt

# Symlinks for model-specific names that the driver probes
ln -sf brcmfmac43455-sdio.bin      brcmfmac43455-sdio.raspberrypi,5-model-b.bin
ln -sf brcmfmac43455-sdio.clm_blob brcmfmac43455-sdio.raspberrypi,5-model-b.clm_blob
ln -sf brcmfmac43455-sdio.txt      brcmfmac43455-sdio.raspberrypi,5-model-b.txt

echo "Firmware downloaded to $DIR/firmware/"
ls -la "$DIR/firmware/"
