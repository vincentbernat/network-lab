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

Cumulus is 4.1.1. Maybe 4.2 is using native Linux support for IPv6
next hops for IPv4 routes.

Documentation for Juniper is available [here][juniper].

[juniper]: https://www.juniper.net/documentation/en_US/junos/topics/topic-map/multiprotocol-bgp.html#id-understanding-redistribution-of-ipv4-routes-with-ipv6-next-hop-into-bgp

```
Aug  3 07:52:52  vmx1 rpd[5394]: bgp_read_v4_update:12338: NOTIFICATION sent to fc00::2:1 (External AS 65100): code 3 (Update Message Error) subcode 9 (error with optional attribute)
Aug  3 07:52:52  vmx1 rpd[5394]: Received malformed update from fc00::2:1 (External AS 65100)
Aug  3 07:52:52  vmx1 rpd[5394]:   Family inet-unicast, prefix 0.0.0.0/0
Aug  3 07:52:52  vmx1 rpd[5394]:   Malformed Attribute MP_REACH(14) flag 0x80 length 42.
``
