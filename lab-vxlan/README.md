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
    CONFIG_BRIDGE=y
    
Most variants are only using VLAN/VNI 100. When it makes sense, some
of them are also using VLAN/VNI 200. Most variants are using only IPv6
except when IPv6 is not supported. In this case, IPv4 is used.

More details are available those two blog posts:

 - [VXLAN & Linux][blog1]
 - [VXLAN: BGP EVPN with Cumulus Quagga][blog2]

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

There is a bug in 4.11 (and less) kernels that prevent this scenario
to work as expected when VLAN are
bridged. A [patch](http://patchwork.ozlabs.org/patch/744963/) is
needed to fix that.

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

    $ ping -c2 2001:db8:fe::13
    PING 2001:db8:fe::13(2001:db8:fe::13) 56 data bytes
    64 bytes from 2001:db8:fe::13: icmp_seq=1 ttl=64 time=0.598 ms
    64 bytes from 2001:db8:fe::13: icmp_seq=2 ttl=64 time=1.02 ms
    
    --- 2001:db8:fe::13 ping statistics ---
    2 packets transmitted, 2 received, 0% packet loss, time 1001ms
    rtt min/avg/max/mdev = 0.598/0.811/1.024/0.213 ms

This may seem a bit odd to have two subnets in the same
"VLAN". However, it's possible to specify a VNI in FDB
entries. Therefore, you could use two separate VNI sharing the same
VXLAN interface.

## Unicast and dynamic L2 entries

The kernel can signal missing L2 entries. We can have a controller add
the entries when the kernel requests them. We use a simple shell
script for this purpose. This is slow and clunky (due to buffering
issues) but illustrates how it works.

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

Unfortunately, there is currently no IPv6 support (and no plan to add
it as Cumulus wants to transition to BGP EVPN), so this lab uses with
IPv4. See
this [issue](https://github.com/CumulusNetworks/vxfld/issues/4).

With a recent version of iproute,
a [patch](https://github.com/CumulusNetworks/vxfld/pull/5) is needed.

## BGP EVPN

There is currently two major solutions on Linux for that:

 - [BaGPipe BGP][] (see also this [article][3]), [adopted by OpenStack][4]
 - [Cumulus Quagga][] (see also this [article][5], this [one][7] and this [one][blog2])
 
See also [RFC 7432][]. We use the second solution. Unfortunately,
VXLAN handling is not compatible with IPv6 yet, so we use
IPv4.

Here are some commands to observe the adjacencies from `vtysh`. First,
which VNI are we interested in?

    S1# show bgp evpn import-rt
    Route-target: 0:100
    List of VNIs importing routes with this route-target:
      100
    Route-target: 0:200
    List of VNIs importing routes with this route-target:
      200

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
    EVPN type-2 prefix: [2]:[ESI]:[EthTag]:[MAClen]:[MAC]
    EVPN type-3 prefix: [3]:[EthTag]:[IPlen]:[OrigIP]
    
       Network          Next Hop            Metric LocPrf Weight Path
    Route Distinguisher: 203.0.113.1:100
    *> [2]:[0]:[0]:[48]:[50:54:33:00:00:09]
                        203.0.113.1                        32768 i
    *> [3]:[0]:[32]:[203.0.113.1]
                        203.0.113.1                        32768 i
    Route Distinguisher: 203.0.113.1:200
    *> [2]:[0]:[0]:[48]:[50:54:33:00:00:09]
                        203.0.113.1                        32768 i
    *> [3]:[0]:[32]:[203.0.113.1]
                        203.0.113.1                        32768 i
    Route Distinguisher: 203.0.113.2:100
    *>i[2]:[0]:[0]:[48]:[50:54:33:00:00:0a]
                        203.0.113.2                   100      0 i
    *>i[2]:[0]:[0]:[48]:[50:54:33:00:00:0b]
                        203.0.113.2                   100      0 i
    *>i[3]:[0]:[32]:[203.0.113.2]
                        203.0.113.2                   100      0 i
    Route Distinguisher: 203.0.113.2:200
    *>i[2]:[0]:[0]:[48]:[50:54:33:00:00:0a]
                        203.0.113.2                   100      0 i
    *>i[3]:[0]:[32]:[203.0.113.2]
                        203.0.113.2                   100      0 i
    Route Distinguisher: 203.0.113.3:100
    *>i[2]:[0]:[0]:[48]:[50:54:33:00:00:0c]
                        203.0.113.3                   100      0 i
    *>i[3]:[0]:[32]:[203.0.113.3]
                        203.0.113.3                   100      0 i
    Route Distinguisher: 203.0.113.3:200
    *>i[2]:[0]:[0]:[48]:[50:54:33:00:00:0c]
                        203.0.113.3                   100      0 i
    *>i[3]:[0]:[32]:[203.0.113.3]
                        203.0.113.3                   100      0 i
    
    Displayed 13 prefixes (13 paths)

For more details, specify a route distinguisher:

    S1# show bgp evpn route rd 203.0.113.3:100
    EVPN type-2 prefix: [2]:[ESI]:[EthTag]:[MAClen]:[MAC]
    EVPN type-3 prefix: [3]:[EthTag]:[IPlen]:[OrigIP]
    
    BGP routing table entry for 203.0.113.3:100:[2]:[0]:[0]:[48]:[50:54:33:00:00:0c]
    Paths: (1 available, best #1)
      Not advertised to any peer
      Route [2]:[0]:[0]:[48]:[50:54:33:00:00:0c] VNI 100
      Local
        203.0.113.3 from 203.0.113.254 (203.0.113.3)
          Origin IGP, localpref 100, valid, internal, bestpath-from-AS Local, best
          Extended Community: RT:65000:100 ET:8
          Originator: 203.0.113.3, Cluster list: 203.0.113.254
          AddPath ID: RX 0, TX 10
          Last update: Thu Mar 30 07:48:40 2017
    
    BGP routing table entry for 203.0.113.3:100:[3]:[0]:[32]:[203.0.113.3]
    Paths: (1 available, best #1)
      Not advertised to any peer
      Route [3]:[0]:[32]:[203.0.113.3]
      Local
        203.0.113.3 from 203.0.113.254 (203.0.113.3)
          Origin IGP, localpref 100, valid, internal, bestpath-from-AS Local, best
          Extended Community: RT:65000:100 ET:8
          Originator: 203.0.113.3, Cluster list: 203.0.113.254
          AddPath ID: RX 0, TX 12
          Last update: Thu Mar 30 07:48:40 2017
    
    
    Displayed 2 prefixes (2 paths) with this RD

There are two types of routes (first digit):

 - type 2 (MAC with IP advertisement route): they enable transmission
   of FDB entries for a given VNI
 
 - type 3 (multicast Ethernet routes): they are here to make
   broadcast, unknown unicast and multicast traffic.

### Interoperability

As for interoperability, the biggest problem is how RD and RT are
computed. A type 2 route contains the following fields:

 - a Route Distinguishier (RD)
 - an Ethernet Segment Identifier (ESI), used when an Ethernet segment
   is multi-homed
 - an Ethernet Tag ID (ETag)
 - a MAC address
 - an optional IP address
 - one or two MPLS labels
 
A type 3 route contains the following fields:

 - a Route Distinguishier (RD)
 - an Ethernet Tag ID (ETag)
 - an IP address
 
Each vendor has its own way to map a VXLAN domain to one of the
attributes. Moreover, each NLRI can have a Route Target
(RT). [RFC 7432][] acknowledges the fact that there is not a unique
way to do it (it presents 3 options, in section 6: VLAN-based service
interface, VLAN bundle service interface and VLAN-aware bundle service
interface). The same options are presented
in [draft-sd-l2vpn-evpn-overlay][] (single subnet per EVPN instance,
multiple subnets per EVPN instance).

Juniper has a lot of material
on [configuring BGP EVPN on the QFX line][10], but far less for the MX
which requires the use of virtual switches. Hopefully, there is
an [Ansible playbook][] for this.

Currently, Cumulus Quagga and Junos don't agree on how to encode
everything. However, they are both flexible enough to accept to speak
to each other no matter what. See configuration in commit c530b4fbb618
for this. The incompatibility is that Cumulus uses AS:VNI for the RT
while JunOS uses AS:VNI' where VNI' is VNI|0x10000000. The current
configuration is done thanks to
this [short patch for Cumulus Quagga][15] to add compatibility.

Here is the output from the Juniper side:

    juniper@S3> show evpn database
    Instance: vxlan
    VLAN  DomainId  MAC address        Active source                  Timestamp        IP address
         100        50:54:33:00:00:0c  203.0.113.1                    Mar 30 07:36:51
         100        50:54:33:00:00:0e  203.0.113.2                    Mar 30 07:36:51
         100        50:54:33:00:00:0f  ge-0/0/1.0                     Mar 30 07:34:00
         200        50:54:33:00:00:0c  203.0.113.1                    Mar 30 07:35:30
         200        50:54:33:00:00:0d  203.0.113.2                    Mar 30 07:36:46
         200        50:54:33:00:00:0f  ge-0/0/1.0                     Mar 30 07:31:17

    juniper@S3> show bridge domain
    
    Routing instance        Bridge domain            VLAN ID     Interfaces
    vxlan                   vlan100                  100
                                                                 ge-0/0/1.0
                                                                 vtep.32769
                                                                 vtep.32770
    vxlan                   vlan200                  200
                                                                 ge-0/0/1.0
                                                                 vtep.32769
                                                                 vtep.32770

    juniper@S3> show bridge mac-table
    
    MAC flags       (S -static MAC, D -dynamic MAC, L -locally learned, C -Control MAC
        O -OVSDB MAC, SE -Statistics enabled, NM -Non configured MAC, R -Remote PE MAC)
    
    Routing instance : vxlan
     Bridging domain : vlan100, VLAN : 100
       MAC                 MAC      Logical                Active
       address             flags    interface              source
       50:54:33:00:00:0c   D        vtep.32769             203.0.113.1
       50:54:33:00:00:0e   D        vtep.32770             203.0.113.2
    
    MAC flags       (S -static MAC, D -dynamic MAC, L -locally learned, C -Control MAC
        O -OVSDB MAC, SE -Statistics enabled, NM -Non configured MAC, R -Remote PE MAC)
    
    Routing instance : vxlan
     Bridging domain : vlan200, VLAN : 200
       MAC                 MAC      Logical                Active
       address             flags    interface              source
       50:54:33:00:00:0c   D        vtep.32769             203.0.113.1
       50:54:33:00:00:0d   D        vtep.32770             203.0.113.2
       50:54:33:00:00:0f   D        ge-0/0/1.0

[Ansible playbook]: https://github.com/JNPRAutomate/ansible-junos-evpn-vxlan/tree/master/roles/overlay-evpn-mx-l3
[BaGPipe BGP]: https://github.com/Orange-OpenSource/bagpipe-bgp
[3]: http://murat1985.github.io/kubernetes/cni/2016/05/15/bagpipe-gobgp.html
[4]: https://docs.openstack.org/developer/networking-bagpipe/
[Cumulus Quagga]: http://github.com/cumulusnetworks/quagga
[5]: https://docs.cumulusnetworks.com/display/DOCS/Ethernet+Virtual+Private+Network+-+EVPN
[7]: https://cumulusnetworks.com/learn/web-scale-networking-resources/whitepapers/Cumulus-Networks-White-Paper-EVPN.pdf
[10]: https://www.juniper.net/techpubs/en_US/release-independent/nce/information-products/pathway-pages/nce/nce-153-vcf-evpn-vxlan-integrating.pdf
[15]: https://github.com/CumulusNetworks/quagga/issues/28#issuecomment-290329435
[RFC 7432]: https://tools.ietf.org/html/rfc7432
[draft-sd-l2vpn-evpn-overlay]: https://tools.ietf.org/html/draft-ietf-bess-evpn-overlay-07
[blog1]: https://vincent.bernat.im/en/blog/2017-vxlan-linux
[blog2]: https://vincent.bernat.im/en/blog/2017-vxlan-bgp-evpn

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
