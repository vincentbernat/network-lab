Lab with Arista vEOS
====================

Arista vEOS is a virtual version of Arista EOS.

Lab
---

This lab is an experiment with VARP. VARP is meant to replace first
hop router redundancy protocols like VRRP and HSRP. Unfortunately,
those protocols are usually working in active/standby mode. VARP is an
attempt to correct that by assigning each virtual router a MAC address
shared by all the devices of the virtual router.

As this is the first time I am playing with Arista EOS, you should not
rely on any configuration files for serious work.

Download
--------

You can download vEOS from [Arista website][]. You need both
`Aboot-veos-serial-x.x.x.iso` and `vEOS-x.xx.xx.vmdk`. Convert the
last one with `qemu-img` to the qcow2 format:

    qemu-img convert -O qcow2 vEOS-x.xx.xx.{vmdk,img}

Symlink them in `images/` as `Aboot-veos.iso` and `vEOS.img`. You can
set them read-only for safety. We use COW to avoid any modification of
the original image.

It seems there is a bug in `Aboot-veos-serial-2.0.8.iso` which makes
it not serial-ready at all. A small shell script,
`images/fix-arista-veos-iso-serial.sh` will fix the ISO. It needs
`guestfish` (available in `libguestfs-tools` package).

Also, the image is shipped with an empty `startup-config`. This
prevent ZTP to work. This can be fixed with the shell script
`images/fix-arista-veos-image.sh`. It also needs guestfish.

[Arista website]: https://www.arista.com/en/support/software-download

Commands
--------

Some commands to check that everything works as expected.

Check that OSPF is OK:

    vEOS1#show ip ospf interface 
    Vlan4094 is up
      Interface Address 192.0.2.1, VRF default, Area 0.0.0.0
      Network Type Point-To-Point, Cost: 10
      Transmit Delay is 1 sec, State P2P
      No Designated Router on this network
      No Backup Designated Router on this network
      Timer intervals configured, Hello 10, Dead 40, Retransmit 5
      Neighbor Count is 1
    Vlan3 is up
      Interface Address 198.51.100.2, VRF default, Area 0.0.0.0
      Network Type Broadcast, Cost: 10
      Transmit Delay is 1 sec, State DR, Priority 1
      Designated Router is 203.0.113.1
      No Backup Designated Router on this network
      Timer intervals configured, Hello 10, Dead 40, Retransmit 5
      Neighbor Count is 0
    Vlan4 is up
      Interface Address 203.0.113.1, VRF default, Area 0.0.0.0
      Network Type Broadcast, Cost: 10
      Transmit Delay is 1 sec, State DR Other, Priority 1
      Designated Router is 203.0.113.12
      Backup Designated Router is 203.0.113.11
      Timer intervals configured, Hello 10, Dead 40, Retransmit 5
      Neighbor Count is 3

    vEOS1#show ip ospf neighbor 
    Neighbor ID     VRF    Pri   State            Dead Time   Address         Interface
    203.0.113.2     default    0   FULL             00:00:32    192.0.2.2       Vlan4094
    203.0.113.2     default    1   2 WAYS/DROTHER   00:00:30    203.0.113.2     Vlan4
    203.0.113.12    default    1   FULL/DR          00:00:39    203.0.113.12    Vlan4
    203.0.113.11    default    1   FULL/BDR         00:00:39    203.0.113.11    Vlan4

Check the virtual IP is here:

    vEOS1(config)#show ip virtual-router 
    IP virtual router is configured with MAC address: 001c.7300.0099
    MAC address advertisement interval: 30 seconds
    Interface  IP Address        Virtual IP Address   Status            Protocol         
    Vlan3      198.51.100.2/24   198.51.100.1         up                up               

Moreover, we can see its MAC address moving from one port to another:

    /usr/bin/vde_switch: MAC 00:1c:73:00:00:99 moved from port 2 to port 1
    /usr/bin/vde_switch: MAC 00:1c:73:00:00:99 moved from port 1 to port 2
    /usr/bin/vde_switch: MAC 00:1c:73:00:00:99 moved from port 2 to port 1
    /usr/bin/vde_switch: MAC 00:1c:73:00:00:99 moved from port 1 to port 2
    /usr/bin/vde_switch: MAC 00:1c:73:00:00:99 moved from port 2 to port 1
    /usr/bin/vde_switch: MAC 00:1c:73:00:00:99 moved from port 1 to port 2

And we can see on C2 that the virtual MAC is never used as a source address:

    23:25:57.451429 50:54:58:5c:b4:7b > 50:54:93:35:24:50, ethertype IPv4 (0x0800), length 98: 203.0.113.11 > 198.51.100.12: ICMP echo request, id 238, seq 7, length 64
    23:25:57.451447 50:54:93:35:24:50 > 00:1c:73:00:00:99, ethertype IPv4 (0x0800), length 98: 198.51.100.12 > 203.0.113.11: ICMP echo reply, id 238, seq 7, length 64
