# WiFi Manager — QNX + Android

## Architecture

```
[Android App] --WiFi Hotspot--> [RPi QNX Host] --> connects to --> [Target WiFi]
                                  |
              wifi_service (TCP :8888 + built-in DHCP server)
```

The RPi runs `wifi_service`, which:
1. Starts `hostapd` on `bcm0` with hotspot `QNX_Config`
2. Starts a **built-in DHCP server** (no dnsmasq needed) on UDP/67
3. Listens on TCP port 8888
4. When Android sends `{"ssid":"...","password":"..."}`, it stops the hotspot
   and connects to the target WiFi via `wpa_supplicant -D qwdi`

---

## Build

```bash
source qnxsdp-env.sh   # QNX 8.0 environment
cd wifi_service
make
```

---

## Run on RPi

```bash
# Stop existing WiFi client
slay -f wpa_supplicant 2>/dev/null
slay -f dhcpcd 2>/dev/null

# Run the service (must be root)
./wifi_service
```

---

## Prerequisites on QNX

Already in the hypervisor image:
- `hostapd` + `hostapd_cli`
- `wpa_supplicant` + `wpa_cli`
- `dhcpcd`
- `qwdi_dhd_sdio` driver for BCM43455

---

## Android App

Open `android_app/` in Android Studio and build.

### Usage
1. Boot RPi, run `wifi_service` — hotspot `QNX_Config` appears
2. Connect Android to `QNX_Config` (password: `qnxconfig123`)
3. Open WiFi Manager app, enter target SSID/password, tap **Send & Connect**

---

## Protocol

**Request:** `{"ssid":"MyWiFi","password":"secret123"}`  
**Response:** `{"status":"success","message":"connected to MyWiFi"}`  
or `{"status":"error","message":"..."}`
