# Testing OSPF convergence time

This uses some random topology. Put some links down, then watch how
much time is needed to settle. There are `monitor.*` files with
timestamps to get the right values. For example:

```
grep -h 23:53: monitor.V* | sort | awk 'NR == 1 { print } END { print }'
```

## Convergence tests with 1500 routers on a 6-core processor

When running on a Ryzen 5 5600X (6-core processor) and using the
provided values (30 VMs and 50 NSs each, hence 1500 routers and each
of them gets 0.4% of a core), we get the following convergence times.

No tuning has been done and notably, we are running in some netlink
overflows. Each router gets 0.4% of a core, which seems quite low.

### Shutting down link between V1 and V30

```
[2021-10-22T23:53:49.059282] [nsid 1]Deleted 2001:db8::1:15 via fe80::5024:98ff:feb0:596b dev dummy0 proto bird metric 32 pref medium
[2021-10-22T23:53:52.056602] [nsid 0]Deleted 2001:db8::28:3 via fe80::6c39:c5ff:fe53:80d6 dev eth0 proto bird metric 32 pref medium
```

### Shutting down link between V1 and V2

```
[2021-10-22T23:58:12.351976] [nsid current]Deleted 2001:db8::2:1 via fe80::5254:33ff:fe00:3 dev eth2 proto bird metric 32 pref medium
[2021-10-22T23:58:15.339387] [nsid 1]2001:db8::22:4 via fe80::b4d1:acff:fe3a:ebe dev eth1 proto bird metric 32 pref medium
```

### Shutting down link between V1 and each NS

```
 23:52 CEST ❱ birdc show ospf neigh
BIRD 2.0.8 ready.
UNDERLAY:
Router ID       Pri          State      DTime   Interface  Router IP
30.0.2.0          1     Full/BDR        35.701  eth1       fe80::5254:33ff:fe00:3c
2.0.2.0           1     Full/DR         31.200  eth2       fe80::5254:33ff:fe00:3
1.0.1.1           1     Full/PtP        39.610  veth0      fe80::f0e1:69ff:fe12:2fb6
1.0.1.24          1     Full/PtP        35.147  veth1      fe80::cc93:9ff:fe79:9672
1.0.1.25          1     Full/PtP        35.626  veth2      fe80::381a:b2ff:fed4:28ae
1.0.1.37          1     Full/PtP        36.506  veth3      fe80::10b2:23ff:fe5b:6251
```

```
[2021-10-23T00:00:39.620211] [nsid current]Deleted 2001:db8::1:1 via fe80::f0e1:69ff:fe12:2fb6 dev veth0 proto bird metric 32 pref medium
[2021-10-23T00:00:41.359045] [nsid 5]2001:db8::1:43 via fe80::30c6:d1ff:feb9:ce26 dev eth1 proto bird metric 32 pref medium
```

```
[2021-10-23T00:01:26.262337] [nsid current]Deleted 2001:db8::1:13 via fe80::cc93:9ff:fe79:9672 dev veth1 proto bird metric 32 pref medium
[2021-10-23T00:01:28.342800] [nsid 5]2001:db8::1:40 via fe80::30c6:d1ff:feb9:ce26 dev eth1 proto bird metric 32 pref medium
```

```
[2021-10-23T00:01:48.148564] [nsid current]Deleted 2001:db8::1:4 via fe80::381a:b2ff:fed4:28ae dev veth2 proto bird metric 32 pref medium
[2021-10-23T00:01:50.328122] [nsid 1]2001:db8::1:47 via fe80::c8ce:2ff:fe16:8798 dev eth2 proto bird metric 32 pref medium
```

```
[2021-10-23T00:02:14.378579] [nsid current]Deleted 2001:db8::1:1 via fe80::10b2:23ff:fe5b:6251 dev veth3 proto bird metric 32 pref medium
[2021-10-23T00:02:16.781860] [nsid 0]Deleted 2001:db8::1:48 via fe80::74bd:27ff:fe7a:15fe dev eth0 proto bird metric 32 pref medium
```

## Same but with only 10 VMs

We get 500 routers and each of them gets 1.2% of a core. We have also
increased rmem to avoid dropped Netlink messages.

### Shutting down link between V1 and V2

```
[2021-10-23T00:19:07.594997] [nsid current]Deleted 2001:db8::2:1 via fe80::5254:33ff:fe00:3 dev eth2 proto bird metric 32 pref medium
[2021-10-23T00:19:09.199965] [nsid 2]2001:db8::8:48 via fe80::5c8e:b8ff:fe49:8304 dev eth2 proto bird metric 32 pref medium
```
