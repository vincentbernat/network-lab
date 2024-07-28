# Fun with confederations AS override

Combining confederations and AS override is a recipe for disaster:

```
RP/0/RP0/CPU0:R2#show bgp ipv6 u 2001:db8::1/128
Sun Jul 14 07:44:20.896 UTC
BGP routing table entry for 2001:db8::1/128
Versions:
  Process           bRIB/RIB  SendTblVer
  Speaker                  38           38
Last Modified: Jul 14 07:44:19.859 for 00:00:01
Paths: (1 available, best #1)
  Advertised IPv6 Unicast paths to peers (in unique update groups):
    2001:db8::3:1
  Path #1: Received by speaker 0
  Advertised IPv6 Unicast paths to peers (in unique update groups):
    2001:db8::3:1
  (64501 64503 64501 64503 64501 64503 64501 64503 64501 64503 64501 64503 64501 64503 64501 64503 64501 64503 64501 64503 64501 64503 64501 64503 64501 64503 64501 64503 64501 64503 64501 64503 64501 64503 64501 64503 64501 64503 64501 64503 64501 64503 64501 64503 64501 64503 64501 64503 64501 64503 64501 64503 64501 64503 64501 64503 64501 64503 64501 64503 64501 64503 64501 64503 64501 64503 64501 64503 64501 64503 64501 64503 64501 64503 64501 64503 64501 64503 64501 64503 64501 64503 64501 64503 64501 64503 64501 64503 64501 64503 64501 64503 64501 64503 64501 64503 64501 64503 64501 64503 64501 64503 64501 64503 64501 64503 64500)
    2001:db8::2:0 from 2001:db8::2:0 (1.0.0.1)
      Origin IGP, metric 0, localpref 100, valid, confed-external, best, group-best
      Received Path ID 0, Local Path ID 1, version 38
```

More details on [this blog post][].

[this blog post]: https://vincent.bernat.ch/en/blog/2024-bgp-endless-aspath
