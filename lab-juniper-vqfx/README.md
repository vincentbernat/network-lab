# Lab with Juniper vQFX

This lab runs a standalone vQFX making some OSPFv3 adjacencies and
also acting as a switch (IRB) on two interfaces.

The PFE is from the Vagrant package for VirtualBox. You need to
extract the qcow image from it. However, the RE is from the
non-Vagrant package. The Vagrant package has its RE with
autoconfiguration disabled...

Default passwords for root are "no" for the PFE and "Juniper" for the
RE.

## OSPF

You can check everything works as expected, from R1:

    $ ip -6 route show
    2001:db8::/64 dev eth0 proto kernel metric 256  pref medium
    2001:db8:aaaa::/64 dev eth1 proto kernel metric 256  pref medium
    2001:db8:b00c::77 via fe80::205:86ff:fe71:1c03 dev eth0 proto bird metric 1024  pref medium
    2001:db8:dead::200 via fe80::5254:33ff:fe00:b dev eth0 proto bird metric 1024  pref medium
    2001:db8:c0ff::42 dev dummy0 proto kernel metric 256  pref medium

From the vQFX:

    root> show ospf3 neighbor
    ID               Interface              State     Pri   Dead
    2.2.2.2          xe-0/0/0.0             Full        1     36
      Neighbor-address fe80::5254:33ff:fe00:b
    1.1.1.1          xe-0/0/0.0             Full        1     35
      Neighbor-address fe80::5254:33ff:fe00:9

## Bridging

From R1, we can ping R2:

    $ ping -c3 2001:db8:aaaa::2
    PING 2001:db8:aaaa::2(2001:db8:aaaa::2) 56 data bytes
    64 bytes from 2001:db8:aaaa::2: icmp_seq=1 ttl=64 time=16.3 ms
    64 bytes from 2001:db8:aaaa::2: icmp_seq=2 ttl=64 time=6.05 ms
    64 bytes from 2001:db8:aaaa::2: icmp_seq=3 ttl=64 time=6.39 ms
    
    --- 2001:db8:aaaa::2 ping statistics ---
    3 packets transmitted, 3 received, 0% packet loss, time 2003ms
    rtt min/avg/max/mdev = 6.051/9.607/16.380/4.792 ms

And we can ping the vQFX:

    $ ping -c3 2001:db8:aaaa::3
    PING 2001:db8:aaaa::3(2001:db8:aaaa::3) 56 data bytes
    64 bytes from 2001:db8:aaaa::3: icmp_seq=1 ttl=64 time=19.2 ms
    64 bytes from 2001:db8:aaaa::3: icmp_seq=2 ttl=64 time=8.66 ms
    64 bytes from 2001:db8:aaaa::3: icmp_seq=3 ttl=64 time=7.70 ms
    
    --- 2001:db8:aaaa::3 ping statistics ---
    3 packets transmitted, 3 received, 0% packet loss, time 2002ms
    rtt min/avg/max/mdev = 7.700/11.874/19.261/5.238 ms
