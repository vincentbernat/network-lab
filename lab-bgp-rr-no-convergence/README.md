# RR and convergence issues

Example of topology where there is no convergence. Each Ri prefers
Bi+1 to reach 2001:db8::1. BGP won't converge to a set of routes.

```
Timestamp: Sun Oct 17 14:10:21 2021 673361 usec
2001:db8::1 via fe80::5254:33ff:fe00:2 dev eth1 proto bird metric 32 pref medium
Timestamp: Sun Oct 17 14:10:21 2021 674409 usec
Deleted 2001:db8::1 via fe80::5254:33ff:fe00:2 dev eth1 proto bird metric 32 pref medium
Timestamp: Sun Oct 17 14:10:21 2021 675487 usec
2001:db8::1 via fe80::5254:33ff:fe00:3 dev eth0 proto bird metric 32 pref medium
Timestamp: Sun Oct 17 14:10:21 2021 676483 usec
Deleted 2001:db8::1 via fe80::5254:33ff:fe00:3 dev eth0 proto bird metric 32 pref medium
Timestamp: Sun Oct 17 14:10:21 2021 677449 usec
2001:db8::1 via fe80::5254:33ff:fe00:2 dev eth1 proto bird metric 32 pref medium
Timestamp: Sun Oct 17 14:10:21 2021 678632 usec
Deleted 2001:db8::1 via fe80::5254:33ff:fe00:2 dev eth1 proto bird metric 32 pref medium
Timestamp: Sun Oct 17 14:10:21 2021 679894 usec
2001:db8::1 via fe80::5254:33ff:fe00:3 dev eth0 proto bird metric 32 pref medium
```

Source (page 4):

```
@article{10.1145/964725.633028,
author = {Griffin, Timothy G. and Wilfong, Gordon},
title = {On the Correctness of IBGP Configuration},
year = {2002},
issue_date = {October 2002},
publisher = {Association for Computing Machinery},
address = {New York, NY, USA},
volume = {32},
number = {4},
issn = {0146-4833},
url = {https://doi.org/10.1145/964725.633028},
doi = {10.1145/964725.633028},
abstract = {The Border Gateway Protocol (BGP) has two distinct modes of operation. External BGP
(EBGP) exchanges reachability information between autonomous systems, while Internal
BGP (IBGP) exchanges external reachability information within an autonomous system.
We study several routing anomalies that are unique to IBGP because, unlike EBGP, forwarding
paths and signaling paths are not always symmetric. In particular, we focus on anomalies
that can cause the protocol to diverge, and those that can cause a router's chosen
forwarding path to an egress point to be deflected by another router on that path.
Deflections can greatly complicate the debugging of routing problems, and in the worst
case multiple deflections can combine to form persistent forwarding loops. We define
a correct IBGP configuration to be one that is anomaly free for every possible set
of routes sent by neighboring autonomous systems. We show that determination of IBGP
configuration correctness is NP-hard. However, we give simple sufficient conditions
on network configurations that guarantee correctness.},
journal = {SIGCOMM Comput. Commun. Rev.},
month = aug,
pages = {17–29},
numpages = {13},
keywords = {internal BGP, border gateway protocol, BGP congfiguration, BGP}
}
```
