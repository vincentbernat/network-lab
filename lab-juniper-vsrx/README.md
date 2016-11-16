Lab with Juniper vSRX
=====================

This is an adaptation of the Firefly lab. Currently, not working.

Lab
---

This lab is quite simple. Two Juniper SRX and two Linux running BIRD
are plugged on the same virtual switch and establish OSPF adjacencies
between them (with BFD for faster convergence times).

    root@SRX2> show ospf neighbor    
    Address          Interface              State     ID               Pri  Dead
    192.0.2.3        ge-0/0/1.0             2Way      3.3.3.3          128    39
    192.0.2.2        ge-0/0/1.0             Full      2.2.2.2            1    31
    192.0.2.1        ge-0/0/1.0             Full      1.1.1.1            1    29
    
    root@SRX2> show ospf route       
    Topology default Route Table:
    
    Prefix             Path  Route      NH       Metric NextHop       Nexthop      
                       Type  Type       Type            Interface     Address/LSP
    1.1.1.1            Intra Router     IP            1 ge-0/0/1.0    192.0.2.1
    2.2.2.2            Intra Router     IP            1 ge-0/0/1.0    192.0.2.2
    3.3.3.3            Intra Router     IP            1 ge-0/0/1.0    192.0.2.3
    192.0.2.0/24       Intra Network    IP            1 ge-0/0/1.0
    198.51.100.101/32  Intra Network    IP            1 ge-0/0/1.0    192.0.2.1
    198.51.100.102/32  Intra Network    IP            1 ge-0/0/1.0    192.0.2.2
    198.51.100.104/32  Intra Network    IP            0 lo0.0
    
    root@SRX2> show bfd session      
                                                      Detect   Transmit
    Address                  State     Interface      Time     Interval  Multiplier
    192.0.2.1                Up        ge-0/0/1.0     1.000     0.200        5   
    192.0.2.2                Up        ge-0/0/1.0     1.000     0.200        5   
    192.0.2.3                Up        ge-0/0/1.0     1.000     0.200        5   
    
    3 sessions, 3 clients
    Cumulative transmit rate 15.0 pps, cumulative receive rate 15.0 pps

The transmit interval for BFD is 200 ms but it can be reduced on real hardware.

Download
--------

You can download it from [Juniper website][]. Choose the KVM
image. Put it in the `images/` directory and name it `junos-vsrx.img`.

[Juniper website]: http://www.juniper.net/us/en/products-services/security/srx-series/vsrx/#evaluation
