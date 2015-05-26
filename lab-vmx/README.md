Lab with Juniper vMX
====================

Almost the same as the lab with vSRX. However, autoconfiguration is
not available, hence the use of expect script for initial
configuration. The root password is `.Linux.`.

Lab
---

This lab is quite simple. Two Juniper vMX and two Linux running BIRD
are plugged on the same virtual switch and establish OSPF adjacencies
between them (with BFD for faster convergence times).

    root@vMX1> show ospf neighbor
    Address          Interface              State     ID               Pri  Dead
    192.0.2.4        ge-0/0/0.0             2Way      4.4.4.4          128    36
    192.0.2.2        ge-0/0/0.0             Full      2.2.2.2            1    38
    192.0.2.1        ge-0/0/0.0             Full      1.1.1.1            1    38
    
    root@vMX1> show ospf route
    Topology default Route Table:
    
    Prefix             Path  Route      NH       Metric NextHop       Nexthop
                       Type  Type       Type            Interface     Address/LSP
    1.1.1.1            Intra Router     IP            1 ge-0/0/0.0    192.0.2.1
    2.2.2.2            Intra Router     IP            1 ge-0/0/0.0    192.0.2.2
    4.4.4.4            Intra Router     IP            1 ge-0/0/0.0    192.0.2.4
    192.0.2.0/24       Intra Network    IP            1 ge-0/0/0.0
    198.51.100.101/32  Intra Network    IP            1 ge-0/0/0.0    192.0.2.1
    198.51.100.102/32  Intra Network    IP            1 ge-0/0/0.0    192.0.2.2
    198.51.100.103/32  Intra Network    IP            0 lo0.0
    198.51.100.104/32  Intra Network    IP            1 ge-0/0/0.0    192.0.2.4
    
    root@vMX1> show bfd session
                                                      Detect   Transmit
    Address                  State     Interface      Time     Interval  Multiplier
    192.0.2.1                Up        ge-0/0/0.0     1.000     0.200        5
    192.0.2.2                Up        ge-0/0/0.0     1.000     0.200        5
    192.0.2.4                Up        ge-0/0/0.0     1.000     0.200        5
    
    3 sessions, 3 clients
    Cumulative transmit rate 15.0 pps, cumulative receive rate 15.0 pps

The transmit interval for BFD is 200 ms but it can be reduced on real hardware.
