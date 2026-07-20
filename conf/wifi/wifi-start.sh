#!/bin/sh
#
# Bring up the RPi5 on-board WiFi (Cypress CYW43455 over SDIO).
#
# Run from the host shell:  sh /scripts/wifi-start.sh
#
# This is deliberately NOT called from the boot script. The host is reached over the wired
# bridge0 (192.168.2.2); a driver that faults while attaching would take io-sock down with
# it and cost you the ssh session. Start it by hand until it is known good.
#
# The driver is mounted into the io-sock that is already running (started in the boot
# script with the wired drivers), rather than restarting io-sock.

WPA_CONF=/etc/wpa_supplicant.conf
DRIVER=devs-qwdi_dhd_sdio-2_11-rpi5.so

# --- preflight -------------------------------------------------------------------
# Every one of these is missing from a stock checkout, so check them by name and say
# which one is absent instead of failing somewhere deep in the driver.

if [ ! -f /etc/firmware/brcmfmac43455-sdio.bin ]; then
    echo "wifi: MISSING firmware /etc/firmware/brcmfmac43455-sdio.bin"
    echo "wifi: see conf/wifi/README.md - firmware is not shipped in this repo"
    exit 1
fi

if ! command -v wpa_supplicant >/dev/null 2>&1; then
    echo "wifi: MISSING wpa_supplicant binary"
    echo "wifi: it is not in qsc_install_packages.list - see conf/wifi/README.md"
    exit 1
fi

if grep -q CHANGEME $WPA_CONF 2>/dev/null; then
    echo "wifi: WARNING $WPA_CONF still has the placeholder SSID/passphrase"
    echo "wifi: association will fail until conf/wifi/wpa_supplicant.conf is filled in"
fi

# --- driver ----------------------------------------------------------------------
echo "wifi: mounting $DRIVER into io-sock ..."
mount -T io-sock $DRIVER || {
    echo "wifi: failed to mount $DRIVER"
    exit 1
}

# The interface name comes from the driver; list what appeared so it is visible in the
# log if it is not the expected one.
echo "wifi: interfaces now present:"
ifconfig -l

WLAN_IF=$(ifconfig -l | tr ' ' '\n' | grep -E '^(wlan|bcm)' | head -1)
if [ -z "$WLAN_IF" ]; then
    echo "wifi: no wlan interface appeared - check 'sloginfo' for driver errors"
    exit 1
fi
echo "wifi: using interface $WLAN_IF"

ifconfig $WLAN_IF up

# --- supplicant + address --------------------------------------------------------
echo "wifi: starting wpa_supplicant on $WLAN_IF ..."
wpa_supplicant -B -i $WLAN_IF -c $WPA_CONF

echo "wifi: requesting a DHCP lease ..."
dhcpcd -bqq $WLAN_IF

echo "wifi: done - check association with 'wpa_cli status' and 'ifconfig $WLAN_IF'"
