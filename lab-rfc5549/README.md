# RFC 5549 tests

On `ping`:

```console
$ ip netns exec eth0 ping -c3 192.0.2.6
PING 192.0.2.6 (192.0.2.6) 56(84) bytes of data.
64 bytes from 192.0.2.6: icmp_seq=1 ttl=61 time=2.55 ms
64 bytes from 192.0.2.6: icmp_seq=2 ttl=61 time=1.80 ms
64 bytes from 192.0.2.6: icmp_seq=3 ttl=61 time=1.94 ms

--- 192.0.2.6 ping statistics ---
3 packets transmitted, 3 received, 0% packet loss, time 2003ms
rtt min/avg/max/mdev = 1.804/2.097/2.552/0.325 ms
```

Cumulus (tested with 4.1.1 and 4.2.0) relies on a hack to have IPv4
routes with IPv6 next hops instead of relying on Linux native
capability for that:

```console
$ ip route show
192.0.2.0/30 dev swp1 proto kernel scope link src 192.0.2.1
192.0.2.4/30 via 169.254.0.1 dev swp2 proto bgp metric 20 onlink
192.0.2.8/30 via 169.254.0.1 dev swp2 proto bgp metric 20 onlink
192.0.2.12/30 via 169.254.0.1 dev swp2 proto bgp metric 20 onlink
$ ip neigh show dev swp2
169.254.0.1 lladdr 50:54:33:00:00:10 PERMANENT proto zebra
fe80::5254:33ff:fe00:10 lladdr 50:54:33:00:00:10 router REACHABLE
```

Documentation for Juniper is available [here][juniper].

[juniper]: https://www.juniper.net/documentation/en_US/junos/topics/topic-map/multiprotocol-bgp.html#id-understanding-redistribution-of-ipv4-routes-with-ipv6-next-hop-into-bgp

```
Aug  3 07:52:52  vmx1 rpd[5394]: bgp_read_v4_update:12338: NOTIFICATION sent to fc00::2:1 (External AS 65100): code 3 (Update Message Error) subcode 9 (error with optional attribute)
Aug  3 07:52:52  vmx1 rpd[5394]: Received malformed update from fc00::2:1 (External AS 65100)
Aug  3 07:52:52  vmx1 rpd[5394]:   Family inet-unicast, prefix 0.0.0.0/0
Aug  3 07:52:52  vmx1 rpd[5394]:   Malformed Attribute MP_REACH(14) flag 0x80 length 42.
```

Sent by Juniper device:

```
Path Attribute - MP_REACH_NLRI
    Flags: 0x90, Optional, Extended-Length, Non-transitive, Complete
    Type Code: MP_REACH_NLRI (14)
    Length: 26
    Address family identifier (AFI): IPv4 (1)
    Subsequent address family identifier (SAFI): Unicast (1)
    Next hop network address (16 bytes)
        Next Hop: fc00::2:2
    Number of Subnetwork points of attachment (SNPA): 0
    Network layer reachability information (5 bytes)
        192.0.2.12/30
            MP Reach NLRI prefix length: 30
            MP Reach NLRI IPv4 prefix: 192.0.2.12
```

Sent by FRR:

```
Path Attribute - MP_REACH_NLRI
    Flags: 0x90, Optional, Extended-Length, Non-transitive, Complete
    Type Code: MP_REACH_NLRI (14)
    Length: 42
    Address family identifier (AFI): IPv4 (1)
    Subsequent address family identifier (SAFI): Unicast (1)
    Next hop network address (32 bytes)
        Next Hop: Unknown address
    Number of Subnetwork points of attachment (SNPA): 0
    Network layer reachability information (5 bytes)
        192.0.2.4/30
            MP Reach NLRI prefix length: 30
            MP Reach NLRI IPv4 prefix: 192.0.2.4
```

So, for some reason, FRR is sending a 32-byte next-hop?

From BIRD:

```
Path Attribute - MP_REACH_NLRI
    Flags: 0x90, Optional, Extended-Length, Non-transitive, Complete
    Type Code: MP_REACH_NLRI (14)
    Length: 42
    Address family identifier (AFI): IPv4 (1)
    Subsequent address family identifier (SAFI): Unicast (1)
    Next hop network address (32 bytes)
        Next Hop: Unknown address
    Number of Subnetwork points of attachment (SNPA): 0
    Network layer reachability information (5 bytes)
        192.0.2.8/30
            MP Reach NLRI prefix length: 30
            MP Reach NLRI IPv4 prefix: 192.0.2.8
```

BIRD too!

In the 32-byte NH, there seems to have two address concat: the address
of the peer and the link-local address (even if the two match).

RFC 5549 says:

> Specifically, this document allows advertising with [RFC4760] of an
> MP_REACH_NLRI with:
>    -  AFI = 1
>    -  SAFI = 1, 2, 4, or 128
>    -  Length of Next Hop Address = 16 or 32
>    -  Next Hop Address = IPv6 address of next hop (potentially followed
>       by the link-local IPv6 address of the next hop).  This field is to
>       be constructed as per Section 3 of [RFC2545].
>    -  NLRI= NLRI as per current AFI/SAFI definition

So, it's allowed. With IPv6, the length can be 16 bytes if the IPv6 is
global or 32 bytes if it is local. In this case, the first hop is the
global one and the second the local one.

RFC 2545 says:

> A BGP speaker shall advertise to its peer in the Network Address of
> Next Hop field the global IPv6 address of the next hop, potentially
> followed by the link-local IPv6 address of the next hop.
>
> The value of the Length of Next Hop Network Address field on a
> MP_REACH_NLRI attribute shall be set to 16, when only a global
> address is present, or 32 if a link-local address is also included in
> the Next Hop field.

So, having a 32-byte next hop is valid.
