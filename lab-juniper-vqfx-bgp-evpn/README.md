# BGP EVPN instead of MC-LAG

This is a simple lab to try to replace an MC-LAG setup with BGP EVPN.
The pair of switches act as an L3 gateway for VLAN 583. However, they
cannot route properly traffic they receive to this VLAN if the local
interface is down as they won't try to issue an ARP request through
the other switch.

To test, set down the interface between QFX2 and R2:

    # ip link set down dev eth1

A ping from R1 to R2, through QFX2 doesn't work anymore:

    # ping -c3 172.27.96.100
    PING 172.27.96.100 (172.27.96.100) 56(84) bytes of data.
    
    --- 172.27.96.100 ping statistics ---
    3 packets transmitted, 0 received, 100% packet loss, time 36ms

We would expect QFX2 to either route the packet to QFX1 or to issue an
ARP request through QFX1. It does neither.

    juniper@QFX2> show arp
    MAC Address       Address         Name                      Interface               Flags
    52:55:0a:00:02:02 10.0.2.2        10.0.2.2                  em0.0                   none
    52:55:0a:00:02:03 10.0.2.3        10.0.2.3                  em0.0                   none
    50:54:33:00:00:0e 169.254.0.1     169.254.0.1               em1.0                   none
    02:05:86:71:dd:00 172.27.96.125   172.27.96.125             irb.583 [vtep.32769]    permanent remote
    02:05:86:71:dd:03 172.27.192.36   172.27.192.36             xe-0/0/0.0              none
    50:54:33:00:00:10 172.27.192.41   172.27.192.41             xe-0/0/1.0              none
    Total entries: 6

According to ATAC, this is the expected behaviour. I need to check the
RFCs. Advertising 172.27.96.96/27 subnet through the underlay network
won't change a thing. This network is directly connected to QFX2, only
one of its IP becomes unavailable due to the local interface down.

Checked with:

 - 18.1R3-S2.5 (also, on real hardware)
 - 17.4R1.16
