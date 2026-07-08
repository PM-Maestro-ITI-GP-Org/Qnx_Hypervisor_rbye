# Qnx Hypervisor

This is built on top of the [qnx hypervisor tutorial](https://gitlab.com/qnx/hypervisor/getting-started)

## How to build

- clone this repo inside the rpi bsp dir

```bash
# u should be in rpi bsp dir
# remove the images dir if it exists
rm -rf images
# clone the repo
git clone https://github.com/PM-Maestro-ITI-GP-Org/Qnx_Hypervisor_rbye images
```

- add the guests
- then make

```bash
make
```

## How to connect

connect using the serial console using the following command

```bash
picocom -b 115200 /dev/ttyUSB0
```

> the default ip of the qnx host is 192.168.2.2

to use ssh connect to the qnx host using the following command

> there is an issue with the ssh conf rn so u can connect using:

- user: `qnxuser`
- password: `qnxuser`

then use `su` to switch to root and use the password `root` (ty gemy <3).

> hopfully this will be fixed in the future.

## How to get the network to work

### Guest to Guest

works out of the box

### Guest to Host

- host steps (u do this for every device)

```bash
# p0 is an example use any number
ifconfig vp0 create
# bind it to the qvm process
vpctl vp0 peer=/dev/qvm/qnx-guest-1/guest_to_host bind=/dev/vdevpeers/vp0

# idk
ifconfig vp0 -txcsum -txcsum6 -tso4 -tso6
# give ur interface an ip
ifconfig vp0 10.0.0.1 netmask 255.255.255.0 up
# enable forwarding
sysctl -w net.inet.ip.forwarding=1
```

- guest steps

```bash
# in the qvmconf file
# create a network device
# use peerfeats = 0
# use peer with the name u created in the host
# and the name is important
vdev virtio-net
    loc 0x1c0c0000
    intr gic:40
    mac aa:bb:cc:dd:ee:f0
    name guest_to_host
    peer /dev/vdevpeers/vp0
    peerfeats 0x00000000
```

### To make the pc work as a router

```bash
# enable forwarding
sysctl -w net.inet.ip.forwarding=1

# replace wlp0s20f3 with your internet interface and enp7s0 with your local network interface <3
sudo iptables -t nat -A POSTROUTING -o wlp0s20f3 -j MASQUERADE
sudo iptables -A FORWARD -i enp7s0 -o wlp0s20f3 -j ACCEPT
sudo iptables -A FORWARD -i wlp0s20f3 -o enp7s0 -m state --state RELATED,ESTABLISHED -j ACCEPT
```

### To make the qnx host route to the pc

> most of this will be automatically done in the build file, this is just a reference for the future.

- tell the host to route the traffic to the pc

```bash
route add default 192.168.2.1
```

- enable forwarding

```bash
sysctl -w net.inet.ip.forwarding=1
```

- create a pf.conf file with the following content (in /etc/)

```bash
nat on bridge0 from 10.0.0.0/24 to any -> (bridge0)
pass in on vp0 # vp0 or vp1 (depennding on the guest)
# pass in on vp1 # if ur usign vp1 (for linux)
pass out on bridge0
```

- enable the pf firewall

```bash
pfctl -f /etc/pf.conf
pfctl -e
```

- and add the route to the guests

```bash
# IN GUESTS
route add default 10.0.0.1 # use the ip of the qnx host
```
