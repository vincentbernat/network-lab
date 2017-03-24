# VXLAN & Linux

This lab explores the implementation of VXLAN with Linux. At the top
of `./setup`, there is the possibility to choose one variant. Only
IPv6 is supported. OSPFv3 is used for the underlay network. It can
takes some time to converge when the lab starts.

Some of the setups described below may seem complex. The major idea is
that for complex setup, you are expected to have some kind of software
to put entries for you.

Due to the use of IPv6, you need a special version of "ip" including
[a special patch](https://patchwork.ozlabs.org/patch/737132/).

The following kernel options are needed:

    CONFIG_DUMMY=y
    CONFIG_VXLAN=y
    CONFIG_PACKET=y
    CONFIG_LWTUNNEL=y

## Multicast

This simply uses multicast to discover neighbors and send BUM
frames. Nothing fancy. Only works if the underlay network is able to
route multicast.

## Unicast and static flooding

All VTEP know their peers and will flood BUM frames to all peers using
static default entries in the FDB. A single broadcast packet is
therefore amplified by the number of VTEP in the network. This can be
quite huge.

## Unicast and static L2 entries

Same as the previous setup, but learning of L2 MAC are disabled in
favour of static entries. The amplification factor stays exactly the
same but the size of the FDB is constrained by the static MAC. Unknown
MAC will be flooded.

## Unicast and static ARP/ND entries

Same as the previous setup, but we remove the static default entries
in the FDB. BUM frames are not flooded anymore but they can't go
anywhere. Static ARP/ND entries are added on each VTEP to make them
reply to ARP/ND traffic. This makes classic L3 traffic work, restrict
the IP and the MAC that can be used.

This is something that would work if you know in advance all MAC/IP
you will use (or have a registry to update them). No amplification
factor. No way to increase the size of a table above some limit. No
multicast/broadcast.

## Unicast and route short circuit

This is an optimization to avoid classic L3 routing when we can
directly L2 switch. The VTEP will not forward the frame to the router
when it knows how to switch it directly to the destination.

In the lab, the router doesn't exist at all, but the host an ND entry
for it to ensure it sends the appropriate frame to the network (the
router should exist when the VTEP doesn't know the destination, this
is just a simplification).

The VTEP notices the MAC address is associated to a router in the FDB
(it is marked "router"). It does a lookup in the neighbor table for
the original destination, notices it knows how to reach it and will
uses this entry (and MAC).

At the end, from `H1`, you can ping `H4`, despite the router being
absent:

    $ ping -c2 2001:db8:fe::13a
    PING 2001:db8:fe::13(2001:db8:fe::13) 56 data bytes
    64 bytes from 2001:db8:fe::13: icmp_seq=1 ttl=64 time=0.598 ms
    64 bytes from 2001:db8:fe::13: icmp_seq=2 ttl=64 time=1.02 ms
    
    --- 2001:db8:fe::13 ping statistics ---
    2 packets transmitted, 2 received, 0% packet loss, time 1001ms
    rtt min/avg/max/mdev = 0.598/0.811/1.024/0.213 ms

## Unicast and dynamic L2 entries

The kernel can signal missing L2 entries. We can have a controller add
the entries when the kernel requests them. We use a simple shell
script for this purpose. This is slow and clunky but illustrates how
it works.

We cannot have a catch-all rule (otherwise, we won't be notified of L2
misses). But we still need to propagate correctly propagate broadcast
and multicast for ARP/ND. No problem with broadcast (except we have a
large amplification factor) but with multicast, many multicast
addresses have to be added in the FDB.

## Unicast and dynamic ARP/ND entries

The kernel can also signal missing L3 entries. By combining this with
the previous approach, we can remove the need of multicast addresses
in the FDB (and the need for amplification). We get a result similar
to the static approach but can request addresses from a registry only
when we need them.

We also use a simple script and it is still slow and clunky.

This needs a [patched kernel](http://patchwork.ozlabs.org/patch/737444/).

## Unicast routing

VXLAN can also be used as a generic IP tunnel (like GRE). For each
route, it's possible to specify an unicast destination and/or a VNI. A
unique VXLAN endpoint can therefore multiplex many tunnels.

To not modify the lab too much, hosts are still believing they are not
on the same subnet and we use ARP/ND proxying for this usage. It's a
secondary issue and you should not pay attention too much to this
detail.

However, VXLAN driver is still checking the destination MAC address
with its own. Therefore, we need some static ARP/ND entries. This
makes this solution a bit cumbersome. It's unclear to me how this
encapsulation feature should work.

## Cumulus vxfld daemon

The previous unicast examples need some additional software to make
them. This example is using [Cumulus vxfld daemon][] which is the
brick behind Cumulus [Lightweight Network Virtualization][] solution.

[Cumulus vxfld daemon]: https://github.com/CumulusNetworks/vxfld
[Lightweight Network Virtualization]: https://docs.cumulusnetworks.com/display/DOCS/Lightweight+Network+Virtualization+-+LNV+Overview

There are two components:

 - the service node daemon (`vxsnd`) that should run on non-VTEP
   servers and will handle registration and optionally BUM frames,
 - the registration daemon (`vxrd`) that should run on VTEP devices.

The are two possible modes:

 - head-end replication: the VTEP devices are handling BUM frames
   directly (duplicating them)
 - service node replication: BUM frames are forwarded to the service
   nodes which forward them to the appropriate VTEP

The two modes are available in the lab.

You need to either install vxfld on your system or in a virtualenv
(`python setup.py install` or `python setup.py develop`). In the later
case, put relative symbolic links in `common/bin` to the virtualenv to
ensure the lab find them.

Unfortunately, there is currently no IPv6 support, so this lab uses
with IPv4. See this [issue](https://github.com/CumulusNetworks/vxfld/issues/4).

With a recent version of iproute,
a [patch](https://github.com/CumulusNetworks/vxfld/pull/5) is needed.

## BGP EVPN

There is currently two major solutions on Linux for that:

 - [BaGPipe BGP][] (see also this [article][3]), [adopted by OpenStack][4]
 - [Cumulus Quagga][] (see also this [article][5] and this [one][7])
 
See also [RFC 7432](https://tools.ietf.org/html/rfc7432). We use the
second solution. Unfortunately, VXLAN handling is not compatible with
IPv6 yet, so we use IPv4.

Here are some commands to observe the adjacencies from `vtysh`. First,
which VNI are we interested in?

    S1# show bgp evpn import-rt
    Route-target: 0:100
    List of VNIs importing routes with this route-target:
      100

how VNI 100 is exported/imported:

    S1# show bgp evpn vni 100
    VNI: 100 (defined in the kernel)
      RD: 203.0.113.1:100
      Originator IP: 203.0.113.1
      Import Route Target:
        65001:100
      Export Route Target:
        65001:100

Then, the "routes" we have:

    S1# show bgp evpn route
    BGP table version is 0, local router ID is 203.0.113.1
    Status codes: s suppressed, d damped, h history, * valid, > best, i - internal
    Origin codes: i - IGP, e - EGP, ? - incomplete
    
       Network          Next Hop            Metric LocPrf Weight Path
    Route Distinguisher: 203.0.113.1:100
    *> [2]:[0]:[0]:[6]:[50:54:33:00:00:08]
                        203.0.113.1                        32768 i
    *> [3]:[0]:[4]:[203.0.113.1]
                        203.0.113.1                        32768 i
    Route Distinguisher: 203.0.113.2:100
    *  [2]:[0]:[0]:[6]:[50:54:33:00:00:09]
                        203.0.113.2                            0 65003 65002 i
    *> [2]:[0]:[0]:[6]:[50:54:33:00:00:09]
                        203.0.113.2                            0 65002 i
    *  [2]:[0]:[0]:[6]:[50:54:33:00:00:0a]
                        203.0.113.2                            0 65003 65002 i
    *> [2]:[0]:[0]:[6]:[50:54:33:00:00:0a]
                        203.0.113.2                            0 65002 i
    *  [3]:[0]:[4]:[203.0.113.2]
                        203.0.113.2                            0 65003 65002 i
    *> [3]:[0]:[4]:[203.0.113.2]
                        203.0.113.2                            0 65002 i
    Route Distinguisher: 203.0.113.3:100
    *  [2]:[0]:[0]:[6]:[50:54:33:00:00:0b]
                        203.0.113.3                            0 65002 65003 i
    *> [2]:[0]:[0]:[6]:[50:54:33:00:00:0b]
                        203.0.113.3                            0 65003 i
    *  [3]:[0]:[4]:[203.0.113.3]
                        203.0.113.3                            0 65002 65003 i
    *> [3]:[0]:[4]:[203.0.113.3]
                        203.0.113.3                            0 65003 i
    
    Displayed 12 out of 12 total prefixes

For more details, specify a route distinguisher:

    S1# show bgp evpn route rd 203.0.113.3:100
    BGP routing table entry for 203.0.113.3:100
    Paths: (2 available, best #2)
      Advertised to non peer-group peers:
      S2(203.0.113.2) S3(203.0.113.3)
    Route [2]:[0]:[0]:[6]:[50:54:33:00:00:0b]
      65002 65003
        203.0.113.3 from S2(203.0.113.2) (203.0.113.2)
          Origin IGP, localpref 100, valid, external, bestpath-from-AS 65002
          Extended Community: RT:65003:100
          AddPath ID: RX 0, TX 12
          Last update: Wed Mar 22 20:59:20 2017
    
    Route [2]:[0]:[0]:[6]:[50:54:33:00:00:0b]
      65003
        203.0.113.3 from S3(203.0.113.3) (203.0.113.3)
          Origin IGP, localpref 100, valid, external, bestpath-from-AS 65003, best
          Extended Community: RT:65003:100
          AddPath ID: RX 0, TX 4
          Last update: Wed Mar 22 20:59:20 2017
    
    Route [3]:[0]:[4]:[203.0.113.3]
      65002 65003
        203.0.113.3 from S2(203.0.113.2) (203.0.113.2)
          Origin IGP, localpref 100, valid, external, bestpath-from-AS 65002
          Extended Community: RT:65003:100
          AddPath ID: RX 0, TX 13
          Last update: Wed Mar 22 20:59:20 2017
    
    Route [3]:[0]:[4]:[203.0.113.3]
      65003
        203.0.113.3 from S3(203.0.113.3) (203.0.113.3)
          Origin IGP, localpref 100, valid, external, bestpath-from-AS 65003, best
          Extended Community: RT:65003:100
          AddPath ID: RX 0, TX 5
          Last update: Wed Mar 22 20:59:20 2017

There are two types of routes (first digit):

 - type 2 (MAC with IP advertisement route): they enable transmission
   of FDB entries for a given VNI
 
 - type 3 (multicast Ethernet routes): they are here to make
   broadcast, unknown unicast and multicast traffic.

The implementation should be interoperable with vendor
implementations. It doesn't work with Juniper one which chokes on the
received Type-3 route:

    Mar 24 06:44:45  S3 rpd[4139]: bgp_rcv_nlri:10354: NOTIFICATION sent to 203.0.113.1 (External AS 65001): code 3 (Update Message Error) subcode 10 (bad address/prefix field), Reason: peer 203.0.113.1 (External AS 65001) update included invalid route 3:203.0.113.1:100::0::/304 (4 of 19)

Wireshark also finds the mentioned update suspicious:

    Border Gateway Protocol - UPDATE Message
        Marker: ffffffffffffffffffffffffffffffff
        Length: 115
        Type: UPDATE Message (2)
        Withdrawn Routes Length: 0
        Total Path Attribute Length: 92
        Path attributes
            Path Attribute - MP_REACH_NLRI
                Flags: 0x90, Optional, Length: Optional, Non-transitive, Complete, Extended Length
                Type Code: MP_REACH_NLRI (14)
                Length: 63
                Address family identifier (AFI): Layer-2 VPN (25)
                Subsequent address family identifier (SAFI): EVPN (70)
                Next hop network address (4 bytes)
                Number of Subnetwork points of attachment (SNPA): 0
                Network layer reachability information (54 bytes)
                    EVPN NLRI: MAC Advertisement Route
                        AFI: MAC Advertisement Route (2)
                        Length: 33
                        Route Distinguisher: 0001cb0071010064 (203.0.113.1:100)
                        ESI: 00 00 00 00 00 00 00 00 00
                        Ethernet Tag ID: 0
                        MAC Address Length: 6
                        MAC Address: 50:54:33:00:00:0b (50:54:33:00:00:0b)
                        IP Address Length: 0
                        IP Address: NOT INCLUDED
                            [Expert Info (Note/Protocol): IP Address: NOT INCLUDED]
                        MPLS Label Stack: 409600, (BOGUS: Bottom of Stack NOT set!)
                    EVPN NLRI: Inclusive Multicast Route
                        AFI: Inclusive Multicast Route (3)
                        Length: 17
                        Route Distinguisher: 0001cb0071010064 (203.0.113.1:100)
                        Ethernet Tag ID: 0
                        IP Address Length: 4
                        IP Address: NOT INCLUDED
                            [Expert Info (Note/Protocol): IP Address: NOT INCLUDED]
                    [Expert Info (Error/Malformed): Invalid EVPN Route Type (203)!]
                        [Invalid EVPN Route Type (203)!]
                        [Severity level: Error]
                        [Group: Malformed]
            Path Attribute - ORIGIN: IGP
                Flags: 0x40, Transitive: Well-known, Transitive, Complete
                Type Code: ORIGIN (1)
                Length: 1
                Origin: IGP (0)
            Path Attribute - AS_PATH: 65001 
                Flags: 0x50, Transitive, Length: Well-known, Transitive, Complete, Extended Length
                Type Code: AS_PATH (2)
                Length: 6
                AS Path segment: 65001
            Path Attribute - EXTENDED_COMMUNITIES
                Flags: 0xc0, Optional, Transitive: Optional, Transitive, Complete
                Type Code: EXTENDED_COMMUNITIES (16)
                Length: 8
                Carried extended communities: (1 community)

Wireshark seems to expect the IP address to not be included, but the
IP address length is 4 and therefore, the remaining bytes are the IP
address. Maybe a bug in Wireshark?

Some other configuration problems may be present (notably in how the
appropriate route targets and route distinguishers are
computed). Until the above problem is fixed, it's difficult to move
forward.

[BaGPipe BGP]: https://github.com/Orange-OpenSource/bagpipe-bgp
[3]: http://murat1985.github.io/kubernetes/cni/2016/05/15/bagpipe-gobgp.html
[4]: https://docs.openstack.org/developer/networking-bagpipe/
[Cumulus Quagga]: http://github.com/cumulusnetworks/quagga
[5]: https://docs.cumulusnetworks.com/display/DOCS/Ethernet+Virtual+Private+Network+-+EVPN
[7]: https://cumulusnetworks.com/learn/web-scale-networking-resources/whitepapers/Cumulus-Networks-White-Paper-EVPN.pdf

# Other considerations

## Security

While VXLAN provides isolation, there is no encryption
builtin. Encryption can be added inside the VXLAN (notably, Linux
supports MACsec since 4.6) or in the underlay network (notably, with
IPsec).

For MACsec, the key exchange should be done through 802.1X (notably
with `wpa_supplicant`). See this [article][6] for how to setup MACsec
with static keys.

[6]: https://developers.redhat.com/blog/2016/10/14/macsec-a-different-solution-to-encrypt-network-traffic/

For IPsec, there are plenty of documentation about that. Since many
peers may be present, opportunistic encryption seems a good
idea. However, implementations for this are scarce.

## MTU & overhead

To avoid any trouble, it's preferable to ensure that the overlay
network MTU is set to 1500. VXLAN overhead is 50 bytes. Therefore, MTU
of the underlay network needs to be 1550. If you use MACsec, the added
overhead is 32 bytes. IPsec overhead depends on many factors. In
transport mode, with AES and SHA256, the overhead is 56 bytes. With
NAT traversal, this is 64 bytes (additional UDP header). In tunnel
mode, this is 72 bytes. See [Cisco IPSec Overhead Calculator Tool][].

[Cisco IPSec Overhead Calculator Tool]: https://cway.cisco.com/tools/ipsec-overhead-calc/
