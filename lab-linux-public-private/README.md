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
