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

Also, from `mixed1`:

```console
$ curl http://203.0.113.11
mixed2
$ curl http://'[2001:db8::11]'
mixed2
```

And from `mixed2`:

```console
$ ip vrf exec public curl http://203.0.113.10
mixed1
$ ip vrf exec public curl http://'[2001:db8::10]'
mixed1
```

For nginx to listen on public, there are several solutions.

1. Allow `0.0.0.0` and `::` to be bound accross VRF:

```sh
sysctl -w net.ipv4.tcp_l3mdev_accept=1
```

2. Make nginx bind on public IP and use `IP_FREEBIND` socket option
   (not available on nginx).

3. Make nginx bind on public IP and use `net.ipv4.ip_nonlocal_bind` sysctl:

```sh
sysctl -w net.ipv4.ip_nonlocal_bind=1
sysctl -w net.ipv6.ip_nonlocal_bind=1
```

4. Execute nginx in public vrf (making it bound only on public IP
   addresses):

```sh
ip vrf exec public nginx -c ...
```
