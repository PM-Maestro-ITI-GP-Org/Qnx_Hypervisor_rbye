# Host WiFi (RPi5 CYW43455 / SDIO)

Status: **all pieces identified and listed; the SDP packages still have to be installed.**

## What is here

| File | Staged as | Purpose |
|---|---|---|
| `qwdi_wifi.conf` | `/etc/qwdi_wifi.conf` | driver config — firmware paths, verbosity |
| `wpa_supplicant.conf` | `/etc/wpa_supplicant.conf` | network SSID/PSK (**placeholder**) |
| `wifi-start.sh` | `/scripts/wifi-start.sh` | manual bring-up: mount driver, supplicant, DHCP |

## Packages

Three packages are needed; all three are now in `qsc_install_packages.list`.

| Package | Provides |
|---|---|
| `...target.driver.cypress_dhd_sdio_rpi5_2_11` | `devs-qwdi_dhd_sdio-2_11-rpi5.so` (was already installed) |
| `...target.driver.cypress_dhd_sdio` | firmware: `brcmfmac43455-sdio.bin` / `.txt` / `.clm_blob` |
| `...target.net.wpa_supplicant_2.11.iosock` | `wpa_supplicant-2.11`, `wpa_cli-2.11`, `wpa_passphrase-2.11`, `libwpactrl-2.11.so.1` |

The `2_11` in the driver name is the wpa_supplicant version it speaks to over QWDI, which is
why the 2.11 package is the right one and not `wpa_supplicant_2.10.iosock`.

### Installing them

The two new packages are additive, so install just those rather than re-running
`make qnx_install`:

```sh
cd /home/maxmaster/data/software/qnxsoftwarecenter
./qnxsoftwarecenter_clt -url https://www.qnx.com/swcenter \
  -destination <repo>/qnx800 \
  -profile com.qnx.cti-$(stat -c '%d_%i' <repo>/) \
  -setExperimentalEnabled=true \
  -installPackage com.qnx.qnx800.target.net.wpa_supplicant_2.11.iosock/0.0.5.00007T202509220833L,com.qnx.qnx800.target.driver.cypress_dhd_sdio/0.0.4.00003T202510060827L
```

> **`make qnx_install` is not the cheap option.** It passes `-cleanInstall`, which removes
> the whole baseline installation before reinstalling all ~330 packages. Use it when you
> want to rebuild the SDP from scratch, not to add two packages.

`cypress_dhd_sdio` is marked *experimental* upstream (both available versions are), hence
`-setExperimentalEnabled=true`. The `qnx_install` target already passes that flag.

Then rebuild: `make`. The build works before and after — everything package-provided is
marked `[+optional]` in `rpi5-hypervisor.build`, so mkifs skips what is missing instead of
failing, and picks it up once installed. Check with:

```sh
grep -c "Warning: Host file" <build log>   # 6 before installing, 0 after
```

## Bringing it up

```sh
sh /scripts/wifi-start.sh
```

Not called from the boot script on purpose: the host is reached over the wired `bridge0` at
192.168.2.2, and a driver faulting during attach would take `io-sock` down and your ssh
session with it. Run it by hand until it is known good.

Fill in the real SSID/passphrase in `wpa_supplicant.conf` first — it ships with
`CHANGEME_SSID` and `wifi-start.sh` warns if the placeholder is still there. Don't commit a
real passphrase: the file is baked into the host IFS on the SD card. Use
`wpa_passphrase "SSID" "pass"` and paste the hashed `psk=` line instead.

## Gotchas

The binaries are all installed with a `-2.11` suffix (`wpa_supplicant-2.11`). The build file
stages them under their plain names, so `/usr/sbin/wpa_supplicant` is what ends up on the
target.

The option names in `qwdi_wifi.conf` (`fw`, `nvram`, `clm_blob`) come from the driver
binary's own usage text. They are **not** the `fw_path`/`nvram_path`/`clm_path` names in the
QNX docs — those document `devs-qwdi_syn_dhd_pcie`, a different driver for the PCIe parts.

`com.qnx.qnx800.target.driver.dhd_utils` is not listed; it provides `wl_sdio` and `dhd_sdio`,
which are the tools for interrogating the radio when association misbehaves. Worth adding if
bring-up needs debugging.
