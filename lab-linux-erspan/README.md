# ERSPAN with Linux

Nothing fancy here: ERSPAN for Linux is just an additional kind of
tunnel.

## Security

The only downside is that if you receive traffic only to
capture/analyze, Linux may act on it and forward it. This can happen
on both sides. Usually, traffic copied from another host is not acted
upon because the MAC address doesn't match. However, an attacker could
have access to the analyzer (from a router) or to the router (from the
analyzer) using this tunnel and specially crafted packet.

Notably, from the analyzer, if you change the tunnel to be symmetric
with the tunnel from the router, you can easily send packets:

    ip link del erspan1
    ip link add dev erspan1 type erspan seq key 30 remote 10.1.2.10 local 10.1.2.11 erspan_ver 1 erspan 30
    ip link set up dev erspan1
    ip route add 8.8.8.1/32 dev erspan1
    ip neigh add 8.8.8.1 lladdr 3e:28:aa:da:53:51 dev erspan1
    ping 8.8.8.1

An easy solution is to ask tc to drop any incoming traffic.
