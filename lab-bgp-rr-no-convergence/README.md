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
