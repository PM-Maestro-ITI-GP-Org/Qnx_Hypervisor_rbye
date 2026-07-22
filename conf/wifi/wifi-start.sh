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
    echo "wifi: install com.qnx.qnx800.target.driver.cypress_dhd_sdio into the SDP,"
    echo "wifi: then rebuild and reflash. See conf/wifi/README.md."
    exit 1
fi

if ! command -v wpa_supplicant >/dev/null 2>&1; then
    echo "wifi: MISSING wpa_supplicant binary"
    echo "wifi: install com.qnx.qnx800.target.net.wpa_supplicant_2.11.iosock into the SDP,"
    echo "wifi: then rebuild and reflash. See conf/wifi/README.md."
    exit 1
fi

if grep -q CHANGEME $WPA_CONF 2>/dev/null; then
    echo "wifi: WARNING $WPA_CONF still has the placeholder SSID/passphrase"
    echo "wifi: association will fail until conf/wifi/wpa_supplicant.conf is filled in"
fi

# --- driver ----------------------------------------------------------------------
# Snapshot the interface list first: rather than guessing the WiFi interface by name
# prefix, take whichever interface is new after the driver attaches. The name is chosen
# by the driver and is not documented anywhere, so matching on "wlan*" was a guess -- and
# a dangerous one to get wrong, since cgem0 (the wired NIC in bridge0) is the ssh path.
IF_BEFORE=" $(ifconfig -l) "
echo "wifi: interfaces before: $IF_BEFORE"

echo "wifi: mounting $DRIVER into io-sock ..."
mount -T io-sock $DRIVER || {
    echo "wifi: failed to mount $DRIVER"
    exit 1
}

echo "wifi: interfaces after:  $(ifconfig -l)"

# Pure-shell diff. The IFS in this image has no 'tr', and busybox-style pipelines are not
# available either, so this uses only shell builtins.
WLAN_IF=
for i in $(ifconfig -l); do
    case "$IF_BEFORE" in
        *" $i "*) ;;            # was already present
        *)        WLAN_IF=$i ;; # new since the mount -> this is the radio
    esac
done

if [ -z "$WLAN_IF" ]; then
    echo "wifi: no new interface appeared after mounting the driver."
    echo "wifi: the driver attached but the radio did not come up - most likely the"
    echo "wifi: firmware failed to load. Driver messages follow:"
    echo "---------------------------------------------------------------"
    sloginfo | tail -40
    echo "---------------------------------------------------------------"
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
