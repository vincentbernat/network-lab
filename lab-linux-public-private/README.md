# Hosts with both public and private IP addresses

The usual solution is to manually add IP rules (like in `mixed1`). The
second solution is to use VRF (like in `mixed2`). The later provide
better isolation.

To test, from `public`, use:

```console
$ curl --interface 1.1.1.1 http://203.0.113.10
mixed1
$ curl --interface 1.1.1.1 http://203.0.113.11
mixed2
$ curl --interface 2606:4700:4700::1111 http://'[2001:db8::10]'
mixed1
$ curl --interface 2606:4700:4700::1111 http://'[2001:db8::11]'
mixed2
```

Both solutions, in `/etc/network/interfaces`:

```interfaces
auto eth0
iface eth0 inet static
  address 10.234.78.10/24
  gateway 10.234.78.1
iface eth0 inet6 static
  address 2001:db8:ff::10/64
  gateway 2001:db8:ff::1
```

First solution, add:

```interfaces
auto eth1
iface eth1 inet static
  address 203.0.113.10/24
  pre-up ip rule add from 203.0.113.10 table 90
  post-down ip rule del from 203.0.113.10 table 90
  up ip route add default via 203.0.113.1 table 90
iface eth1 inet6 static
  address 2001:db8::10/24
  pre-up ip -6 rule add from 2001:db8::10 table 90
  post-down ip -6 rule del from 2001:db8::10 table 90
  up ip -6 route add default via 2001:db8::1 table 90
```

Second solution, add:

```interfaces
auto public
iface public inet manual
  pre-up ip link add $IFACE type vrf table 90
  post-down ip link del dev $IFACE

auto eth1
iface eth1 inet static
  address 203.0.113.11/24
  pre-up ip link set master public dev $IFACE
  up ip route add default via 203.0.113.1 table 90
iface eth1 inet6 static
  address 2001:db8::11/24
  pre-up ip link set master public dev $IFACE
  up ip -6 route add default via 2001:db8::1 table 90
```
