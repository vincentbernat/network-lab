# FRR EVPN VXLAN and VRF leaking

This is lab using FRR and EVPN VXLAN to leak Type 5 prefixes between two VRF. It
uses a single VXLAN device. It requires a recent kernel (5.18+) and a recent FRR
(9+).

There are two VRF and each one is leaked into the other. The difficulty is that
FRR should setup the next hop from one VRF to the VNI of the other VRF:

```
R2# show ip bgp vrf vrf2 10.0.10.0/24
BGP routing table entry for 10.0.10.0/24, version 2
Paths: (1 available, best #1, vrf vrf2)
  Not advertised to any peer
  Imported from 1.1.1.1:2:[5]:[0]:[24]:[10.0.10.0], VNI 100
  Local
    100.64.0.1(R1) from R1(100.64.0.1) (1.1.1.1) announce-nh-self
      Origin incomplete, metric 0, localpref 100, valid, internal, bestpath-from-AS Local, best (First path received)
      Extended Community: RT:64600:100 ET:8 Rmac:aa:bb:cc:00:00:01
      Last update: Thu Aug 24 21:50:12 2023
R2# show ip route vrf vrf2 10.0.10.0/24
Routing entry for 10.0.10.0/24
  Known via "bgp", distance 200, metric 0, vrf vrf2, best
  Last update 00:02:13 ago
  * 100.64.0.1, via vx0(vrf default) onlink, label 100, weight 1
```

Notice that the RT is `RT:64600:100` which is associated to VNI 100. This is
correctly translated: the route is in `vrf2` with a label `100` (associated to
`vrf`). From a kernel point of view:

```
 21:54 CEST ❱ ip  route show table 200 10.0.10.0/24
10.0.10.0/24 nhid 30  encap ip id 100 src 0.0.0.0 dst 100.64.0.1 ttl 0 tos 0 via 100.64.0.1 dev vx0 proto bgp metric 20 onlink
```

It uses `encap ip id 100` with the single VXLAN device as a target.

Also see issue [#14259][].

[#14259]: https://github.com/FRRouting/frr/issues/14259
