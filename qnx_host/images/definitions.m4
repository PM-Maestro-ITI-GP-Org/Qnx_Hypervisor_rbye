## Boot parameters
define(`__LD_QNX__',            `ldqnx-64.so.2')
define(`__BOOT_ADDR__',         `0x80000')
define(`__ARCH__',              `aarch64le')
define(`__TYPE__',              `raw')
define(`__COMPRESS_ATTR__',     `-compress')
define(`__PROCNTO_MODULES__',   `')
define(`__STARTUP__',           `startup-bcm2712-rpi5')
define(`__STARTUP_OPTS__',      `-v -u reg -a -W 5000')
define(`__PROCNTO__',           `procnto-smp-instr')
define(`__PROCNTO_OPTS__',      `-v')

## Console
define(`__CONSOLE__',           `__DEVC_SER10_DEV__')

## ENV profile, use to overwrite the common /etc/profile
#define(`__ENV_PROFILE_FILE__',  `etc/profile')
#define(`__PROFILE_CFG__',       `/__ENV_PROFILE_FILE__ = {
#export HOME=/
#export SYSNAME=nto
#export TERM=qansi
#export PATH=/proc/boot:/sbin:/bin:/usr/bin:/usr/sbin:/usr/libexec
#export LD_LIBRARY_PATH=/proc/boot:/lib:/usr/lib:/lib/dll:/lib/dll/pci
#}')

## Audio driver

## Block driver
define(`__BLOCK_DRVR__', `devb-sdmmc-bcm2712')

#define(`__DEVB_AHCI_START__', `')

#define(`__DEVB_EIDE_DRVR__', `')
#define(`__DEVB_EIDE_OPTS__', `')
#define(`__DEVB_EIDE_DEV__', `')

#define(`__DEVB_NVME_DRVR__', `')
#define(`__DEVB_NVME_OPTS__', `')
#define(`__DEVB_NVME_DEV__', `')

#define(`__DEVB_RAM_DRVR__', `')
#define(`__DEVB_RAM_OPTS__', `')
#define(`__DEVB_RAM_DEV__', `')

define(`__DEVB_SDMMC_DRVR__', `devb-sdmmc-bcm2712')
define(`__DEVB_SDMMC_OPTS__', `sdio addr=0x1000fff000,irq=305 disk name=sd')
define(`__DEVB_SDMMC_DEV__', `/dev/sd0')

define(`__DEVB_SDMMC_START__', `
    #######################################################################
    ## SD memory card / eMMC driver
    ## Note: "bmstr_base" option is required for busmaster memory
    ## address translation
    #######################################################################
    display_msg "Starting SDMMC driver (/dev/sd0)"
    __DEVB_SDMMC_DRVR__ __DEVB_SDMMC_OPTS__
    waitfor __DEVB_SDMMC_DEV__
')

define(`__DEVB_DRVR_START__', `
__DEVB_SDMMC_START__
')

## Network driver
define(`__NET_DRVR__', `devs-cgem.so')
define(`__NET_OPTS__', `-m fdt -d cgem')
define(`__NET_DEV__', `cgem0')

## The io-sock use dhcpcd utility by default, uncomment the line below to use dhclient utility
#define(`__NET_DHCLIENT_SUPPORT__', `')

## WIFI Network driver(BCM4339)
#define(`__WIFI_START__', `
    #######################################################################
    ## WIFI Network driver(BCM4339)
    #######################################################################
    #display_msg Starting WIFI Network driver...
    #mount -T io-pkt -o fw=/etc/hotspot/firmware.bin,clm_blob=/etc/hotspot/firmware.clm_blob /lib/dll/devnp-qwdi-2.5_bcm4339-rpi4.so
    #create a wpa_supplicant.conf file in /tmp directory
    #wpa_supplicant -D qwdi -i bcm0 -c /tmp/wpa_supplicant.conf -C /tmp/ &
    #dhclient -m -lf /dev/shmem/dhclient.leases -pf /dev/shmem/dhclient.pid -nw bcm0
#')

## USB host driver
define(`__USB_HOST_DRVR__', `devu-hcd-dwc3-xhci.so')

define(`__USB_START__', `
    #######################################################################
    ## USB OTG Host Controller driver
    #######################################################################
    sh /proc/boot/usb_start.sh
')

## Persistent storge
#define(`__PERSISTENT_STORAGE_DEVICE__', `/dev/sd0t179')
#define(`__PERSISTENT_STORAGE_MOUNT_POINT__', `')
#define(`__PERSISTENT_STORAGE_MOUNT_OPTS__', `-o sync=optional')
#define(`__PERSISTENT_STORAGE_START__', `')
#define(`__PERSISTENT_STORAGE_FILES__', `')

## Serial driver
define(`__DEVC_DRVR__', `__DEVC_SER_DRVR__')
define(`__DEVC_SER_DRVR__', `devc-serpl011-rpi5')
define(`__DEVC_SER10_OPTS__', `-b115200 -c44236800 -e -F -u10 -D 0x107d001000^2,153')
define(`__DEVC_SER10_DEV__', `/dev/ser10')

define(`__DEVC_START__', `
    #######################################################################
    ## Debug console serial driver
    #######################################################################
    display_msg "Starting serial driver (__DEVC_SER10_DEV__)"
    __DEVC_SER_DRVR__ __DEVC_SER10_OPTS__
')

## I2C driver
define(`__I2C_DRVR__', `i2c-dwc-rpi5')
define(`__I2C_OPTS__', `-p0x1f00074000 -c200000000 -q0xa8 --u1')
define(`__I2C_DEV__', `/dev/i2c1')

## SPI driver
define(`__IO_SPI_DRVR__', `spi-dwc')

define(`__IO_SPI_CFG_CONTENTS__', `
[globals]
verbose=5

[bus]
busno=0
name=spi0
base=0x1f00050000
irq=179
input_clock=200000000
bs=cs_max=1

[dev]
parent_busno=0
devno=0
name=dev0
clock_rate=100000000
cpha=1
cpol=0
bit_order=msb
word_width=32
idle_insert=1
')

## Random
#define(`__RANDOM_DRVR__', `')
#define(`__RANDOM_DRVR_OPTS__', `')

## WDT kick
define(`__WDT_DRVR__', `wdtkick')
define(`__WDT_OPTS__', `-W0x24:0x5A028E4C -W0x1c:0x5A000020')

define(`__WDT_START__', `
    ############################################################################################
    ## WatchDog utility
    ## If startup is given -W parameter then the wdtkick utility MUST be uncommented below
    ############################################################################################
    display_msg "Starting wdtkick ..."
    __WDT_DRVR__ __WDT_OPTS__
')

## Customize script
define(`__CUSTOMIZE_SCRIPT_NAME__', `/scripts/board_startup.sh')
#define(`__CUSTOMIZE_SCRIPT_START__', `')
#define(`__CUSTOMIZE_SCRIPT_FILES__', `')

## PCI driver
define(`__PCI_HW_DRVR__', `pci_hw-bcm2712-rpi5.so')
define(`__PCI_HW_MODULE__', `pci_hw-bcm2712-rpi5.so')
define(`__PCI_BUS_SCAN_LIMIT__', `3')
#define(`__PCI_MODULE_BLACKLIST__', `')
define(`__PCI_OPTS__', `--bus-scan-limit=__PCI_BUS_SCAN_LIMIT__ -c')
define(`__PCI_HW_CONFIG_FILE__', `/etc/system/config/pci/pci_hw-bcm2712-rpi5.cfg')
define(`__PCI_HW_CFG__',`
[uid=0 gid=0 perms=0444] __PCI_HW_CONFIG_FILE__ = {
######################################  S E C T I O N  #########################################
##
[PI5]
##
## Pi 5 PCIe HW module configuration
## --------------------------------
##

## PCIe gen speed configuration for the extension port
MAX_GEN_SPEED=3
}')

define(`__PCI_START__', `
    ############################################################################################
    ## PCIe driver
    ############################################################################################
    ksh /proc/boot/pci_server_start.sh
')


## Board specific files
#define(`__BOARD_EARLY_START__', `')
define(`__BOARD_LATE_START__', `
    # ksh /proc/boot/uart4_console.sh
    ksh /proc/boot/i2c_start.sh
    ksh /proc/boot/fan_start.sh
')

define(`__BOARD_FILES__', `

################################################################################################
## Script for launching the io-usb-otg
################################################################################################
pci_server_start.sh = {
#!/bin/sh
    echo "Starting PCI Server ..."
    PCI_HW_MODULE=/lib/dll/pci/pci_hw-bcm2712-rpi5.so
    PCI_SLOG_MODULE=/lib/dll/pci/pci_slog2.so
    PCI_DEBUG_MODULE=/lib/dll/pci/pci_debug2.so
    pci-server __PCI_OPTS__
    waitfor /dev/pci

    # use msix-rp1 to setup RP1 USB, ethernet, UART2, UART4, I2C, SPI IRQs
    # 1. MSIX vector and MSXI capability
    # 2. RPI MSIX_CFG_<6,8,19,31,36,43,45>
    # 3. MIP interrupt controller
    sleep 1
    msix-rp1 -i 6,8,19,31,36,43,45 &
}

usb_start.sh = {
#!/bin/sh
  echo "Starting USB xHCI controller in the host mode (/dev/usb/*)..."

  gpio-rp1 set 42 a2 pn
  gpio-rp1 set 43 a2 pu

  io-usb-otg -v -d dwc3-xhci ioport=0x1f00200000,irq=0xbf,iosize=0x100000,bmstr=0x1000000000,ioport=0x1f00300000,irq=0xc4,iosize=0x100000,bmstr=0x1000000000
  waitfor /dev/usb/io-usb-otg 10
}

uart4_console.sh = {
  #!/bin/sh
  echo "Start uart4 (gpio12-pin32, gpio13-pin33) console"

  gpio-rp1 set 12 a2 pn
  gpio-rp1 set 13 a2 pu

  devc-serpl011-rpi5 -b115200 -v -c50000000 -e -F -u4 0x1f00040000,0xcd &
  waitfor /dev/ser4
  on -t /dev/ser4 /bin/sh
}

uart2_console.sh = {
  #!/bin/sh
  echo "Start uart2 (gpioi4-pin7, gpio5-pin29) console"

  gpio-rp1 set 4 a2 pn
  gpio-rp1 set 5 a2 pu

  devc-serpl011-rpi5 -b115200 -v -c50000000 -e -F -u2 0x1f00038000,0xcb &
  waitfor /dev/ser2
  on -t /dev/ser2 /bin/sh
}

################################################################################################
## Script for network
################################################################################################
net_start.sh = {

#!/bin/sh

# config ETH_RST_N/GPIO32 pin
gpio-rp1 set 32 op pd dh

############################################################################################
## Network driver
############################################################################################
echo "Starting networking ..."
io-sock -m phy ifdef(`__NET_OPTS__', `__NET_OPTS__') ifdef(`__USB_HOST_DRVR__', `__NET_USB_OPTS__') ifdef(`__NET_WIFI_OPTS__', `__NET_WIFI_OPTS__')

echo "Starting DHCP client ..."
dhcpcd -bqq

# Run wpa_supplicant with configuration file /etc/system/config/wpa_supplicant.conf
# to connect to WiFi network
# wpa_supplicant -Dqwdi -i bcm0 -c /etc/system/config/wpa_supplicant.conf &

############################################################################################
## SSH Server
############################################################################################
/proc/boot/.ssh-server.sh

############################################################################################
## REMOTE_DEBUG: gdb or Momentics
##  - refer to the help documentation for the gdb, qconn and the IDE
##    for more information on remote debugging
##  - the commands shown require that NETWORK is enabled too
############################################################################################
devc-pty
qconn

}

################################################################################################
## Script for wifi support - use second io-sock instance
################################################################################################
# io-sock -o prefix=/wifi -o config=/etc/io-sock.conf -d qwdi_dhd_sdio-2_11-rpi5
# SOCK=/wifi dhcpcd -bqq
# SOCK=/wifi wpa_supplicant -D qwdi -i bcm0 -c /tmp/wpa_supplicant.conf &amp;
# SOCK=/wifi /proc/boot/.ssh-server.sh
# SOCK=/wifi ifconfig
# example of =/tmp/wpa_supplicant.conf
#   ctrl_interface=/var/run/wpa_supplicant
#   network={
#   # the SSID of a public Wi-Fi networks
#   ssid="open_ssid__-example"
#   key_mgmt=NONE

################################################################################################
## Script for I2C
################################################################################################
i2c_start.sh = {
#!/bin/sh

echo "Starting I2C driver"
gpio-rp1 set 2 a3 pu
gpio-rp1 set 3 a3 pu
__I2C_DRVR__ __I2C_OPTS__

}

################################################################################################
## fan control
################################################################################################
fan_start.sh = {
#!/bin/sh

echo "Starting fan control driver"

# gpio 45 is congiured as PWM1_CHAN3 to drive fan
gpio-rp1 set 45 a0 pd
fan-rpi5
waitfor /dev/fan
echo 200 > /dev/fan
}

################################################################################################
## GPIO utility
################################################################################################
/bin/gpio-aon-bcm=gpio-aon-bcm
/bin/gpio-bcm=gpio-bcm
/bin/gpio-rp1=gpio-rp1

################################################################################################
## Mailbox utility
################################################################################################
/bin/mbox-bcm=mbox-bcm

################################################################################################
## RPI msix config utility
################################################################################################
/sbin/msix-rp1=msix-rp1

################################################################################################
## fan control resource manager
################################################################################################
/sbin/fan-rpi5=fan-rpi5

################################################################################################
## Wifi support
################################################################################################
/etc/io-sock.conf = {
hw.dhdsdio.dev0="rpi5"
}

/etc/qwdi_wifi.conf = {
fw=/etc/firmware/firmware.bin
nvram=/etc/firmware/nvram.txt
clm_blob=/etc/firmware/firmware.clm_blob
sdio_baseaddr=0x1001100000
sdio_irq=306
drv_supp=7
key_delay=5
sdio_verbose=0
dhd_verbose=0
}

/lib/dll/devs-qwdi_dhd_sdio-2_11-rpi5.so=devs-qwdi_dhd_sdio-2_11-rpi5.so
/usr/sbin/wpa_cli=wpa_cli-2.11
/usr/sbin/wpa_passphrase=wpa_passphrase-2.11
/usr/sbin/wpa_supplicant=wpa_supplicant-2.11
[search=../install/etc${PFS}${QNX_TARGET}/etc] /etc/system/config/wpa_supplicant.conf=wpa_supplicant-2.11.conf

/usr/sbin/hostapd=hostapd-2.11
/usr/sbin/hostapd_cli=hostapd_cli-2.11

/usr/sbin/wl=wl_sdio

[search=../install/etc${PFS}${QNX_TARGET}/etc] /etc/firmware/firmware.bin=firmware/bcm43455_firmware_pkg/brcmfmac43455-sdio.bin
[search=../install/etc${PFS}${QNX_TARGET}/etc] /etc/firmware/nvram.txt=firmware/bcm43455_firmware_pkg/brcmfmac43455-sdio.txt
[search=../install/etc${PFS}${QNX_TARGET}/etc] /etc/firmware/firmware.clm_blob=firmware/bcm43455_firmware_pkg/brcmfmac43455-sdio.clm_blob

################################################################################################
## none official or private utils for debugging purpose, should be removed from official build
################################################################################################
')

