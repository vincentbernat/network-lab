# Routing loop with BGP RR

This is a tentative to exhibit a routing loop with BGP RR. TL;DR, I
was not able to do that on the topology. To introduce a loop, you need
to have a router receive routes from one RR and another router from
another RR. This is broken by design. All routers should peer with all
RR and a link failure should have the same effects on all sessions or
be in the forwarding path.

[Ivan Pepelnjak][] shows an example when using RR leads to loops. It
also says:

> Although the above topology looks utterly silly, you might get
> dangerously close to it in a well-designed network after one or more
> IBGP session failures.

You don't need RR to have loops if you allow iBGP sessions to be
broken while the underlying IGP is working just fine. Just build two
different paths to a target, full-mesh iBGP on loopbacks and break the
iBGP session between the penultimate hop and the last hop and you'll
have a routing loop because the router with the broken iBGP session
will believe it should use the longer path while the one before it
knows it should use the shorter path.

This seems more an argument to only use eBGP sessions for underlay and
overlay (IPv4/IPv6 unicast for underlay, VPNv4/VPNv6 for overlay).

[Ivan Pepelnjak]: https://blog.ipspace.net/2013/10/can-bgp-route-reflectors-really.html
