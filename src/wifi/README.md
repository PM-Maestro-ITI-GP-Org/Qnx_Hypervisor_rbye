# WiFi Setup for QNX Hypervisor (Raspberry Pi 5)

## 1. QNX Software Center Packages

Install these packages via QNX Software Center for the **host** build:

| Package | Purpose |
|---------|---------|
| `com.qnx.qnx800.target.utils.base.w` | WiFi utilities (`wpa_supplicant`, `wpa_cli`, `wpa_passphrase`) |
| QWiFi DHD SDIO driver | `devs-qwdi_dhd_sdio-2_11-rpi5.so` (included in QNX BSP for RPi5) |

Add `com.qnx.qnx800.target.utils.base.w` to `qsc_install_packages.list`.

---

## 2. Firmware

Three files for the BCM43455 WiFi chip, downloaded by `fetch-firmware.sh`:

```
src/wifi/firmware/
  brcmfmac43455-sdio.bin                                    ← firmware binary
  brcmfmac43455-sdio.clm_blob                                ← CLM blob
  brcmfmac43455-sdio.txt                                     ← NVRAM config
  brcmfmac43455-sdio.raspberrypi,5-model-b.bin  → symlink to .bin
  brcmfmac43455-sdio.raspberrypi,5-model-b.clm_blob → symlink to .clm_blob
  brcmfmac43455-sdio.raspberrypi,5-model-b.txt   → symlink to .txt
```

Run once before building:
```bash
cd src/wifi && ./fetch-firmware.sh
```

---

## 3. Host Configuration

All changes are in `qnx_host/images/rpi5-hypervisor.build`.

### 3a. Network driver (io-sock)

Start the network stack with the QWiFi DHD SDIO driver:

```
io-sock ... -m filter -o config=/etc/io-sock.conf -d qwdi_dhd_sdio-2_11-rpi5 &
```

### 3b. io-sock.conf

```
hw.dhdsdio.dev0="rpi5"
dev.qwdi_wifi.conf="/etc/wifi/qwdi_wifi.conf"
```

### 3c. qwdi_wifi.conf

```
fw=/etc/wifi/firmware/brcmfmac43455-sdio.raspberrypi,5-model-b.bin
nvram=/etc/wifi/firmware/brcmfmac43455-sdio.raspberrypi,5-model-b.txt
clm_blob=/etc/wifi/firmware/brcmfmac43455-sdio.raspberrypi,5-model-b.clm_blob
sdio_baseaddr=0x1001100000
sdio_irq=306
drv_supp=7
key_delay=5
sdio_verbose=0
dhd_verbose=0
qwdi_dbg_level=0
```

### 3d. wpa_supplicant.conf

Place in `qnx_host/install/etc/wpa_supplicant.conf` (referenced in build file):

```
network={
    ssid="YOUR_SSID"
    psk="YOUR_PASSWORD"
}
```

### 3e. .wifi_start.sh

A startup script that:
1. Polls for `bcm0` interface (up to 10 s)
2. Runs `wpa_supplicant` with `-D qwdi -i bcm0`
3. Retries association up to 3 times
4. Runs `dhcpcd -b bcm0` to get an IP
5. Enables PF and loads NAT rules

### 3f. PF (packet filter) — NAT for guests

```
ext_if = "bcm0"
int_if = "vp0"
guest_net = "10.0.0.0/24"

set skip on lo
set skip on vp0
set skip on vp1

nat on $ext_if inet from $guest_net to any -> ($ext_if) round-robin
pass out on $ext_if from $guest_net to any
pass out on $ext_if
```

Load with:
```
pfctl -e
pfctl -R -f /etc/pf.conf
pfctl -N -f /etc/pf.conf
```

### 3g. Kernel routing

```
sysctl -w net.inet.ip.forwarding=1
```

---

## 4. Guest Configuration (Guest 1)

Changes in `qnx_guests/images/guest-1/qnx800-guest-1.build`.

### 4a. Static IP

Assign `10.0.0.2/24` on `vtnet0` (the vdevpeer link to the host):

```
ifconfig vtnet0 10.0.0.2/24
```

### 4b. Default route

```
route add default 10.0.0.1
```

`10.0.0.1` is the host's `vp0` interface (vdevpeer bridge), which NATs traffic out through `bcm0` (WiFi).

### 4c. Disable dhcpcd

After setting the static route, kill dhcpcd so it doesn't overwrite it:

```
slay -f dhcpcd 2>/dev/null
```

---

## 5. Summary Diagram

```
                      WiFi (bcm0)
                          ↑ NAT (pf)
                       Host QNX
                    10.0.0.1 (vp0)
                          │
                    vdevpeer link
                          │
                10.0.0.2 (vtnet0)
                     Guest 1 QNX
                          │
                   Motor Data Producer
                          │
                      MQTT broker
                          │
                     Qt GUI (laptop)
```

Guest 1 sends traffic via `10.0.0.1`, the host NATs it to `bcm0`'s DHCP-assigned IP, and responses are routed back.
