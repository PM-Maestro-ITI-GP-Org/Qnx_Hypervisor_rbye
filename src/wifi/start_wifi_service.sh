#!/bin/sh
# WiFi Auto-Config Service — replaces old .wifi_start.sh wpa_supplicant logic
# Place wifi_service binary in /proc/boot or /sbin

echo "=== WiFi: waiting for bcm0 interface... ==="
i=0
while [ $i -lt 20 ]; do
    ifconfig bcm0 >/dev/null 2>&1 && break
    sleep 0.5
    i=$((i+1))
done

ifconfig bcm0 up
mkdir -p /var/run/wpa_supplicant

echo "=== WiFi: starting wifi_service ==="
wifi_service &

echo "=== WiFi: wifi_service started ==="
