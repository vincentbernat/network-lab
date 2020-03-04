# vQFX lab with EVPN multihoming and per-device IP addresses

When IP addresses are not available on both members, they may be
unreachable from the host device.

Try with:

```console
$ fping  172.27.{7,8,9,10,11,12,13,14,15}.1 2> /dev/null
172.27.7.1 is alive
172.27.9.1 is alive
172.27.11.1 is alive
172.27.13.1 is alive
172.27.15.1 is alive
172.27.8.1 is unreachable
172.27.10.1 is unreachable
172.27.12.1 is unreachable
172.27.14.1 is unreachable
```

For some reasons, BGP sessions are able to be established while fping
cannot work.
