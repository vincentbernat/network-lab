# VXLAN & Linux

This lab explores the implementation of VXLAN with Linux. At the top
of `./setup`, there is the possibility to choose one variant. Only
IPv6 is supported. OSPFv3 is used for the underlay network. It can
takes some time to converge when the lab starts.

Some of the setups described below may seem complex. The major idea is
that for complex setup, you are expected to have some kind of software
to put entries for you.

Due to the use of IPv6, you need a special version of "ip" including
[a special patch][https://patchwork.ozlabs.org/patch/737132/].

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

This needs a [patched kernel][http://patchwork.ozlabs.org/patch/737444/].
