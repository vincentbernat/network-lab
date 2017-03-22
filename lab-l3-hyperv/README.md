# L3 routing on the hypervisor

The goal of this lab is to demonstrate how an hypervisor could be
turned into a router while the VM still think they share some L2
subnet.

The hypervisors distribute routes to VM using BGP (through a pair of
route reflectors) on two distinct L2 layers. ARP proxying is used to
let the hypervisors answer to ARP requests for VM on other hypervisors

## IPv6

This lab is also compatible with IPv6 but there are small drawbacks:

 - BIRD doesn't support correctly IPv6 ECMP routes until 1.6.1. If you
   have an older version, comment the `merge path yes` directive in
   `bird-common/rr-client6.conf`. This way, only one route gets
   installed.

 - NDP proxying in Linux requires the declaration of all IP that
   should be proxied. To avoid that, a userland proxy (ndppd) is
   used. Another option would be to program those IP using some daemon
   listening to netlink messages for added/removed routes.

 - Hypervisor are not expected to do regular IPv6 traffic (they can do
   IPv4 for internal use using the private IP addresses). If they do,
   they'll use a non-loopback IP that would depend on the
   corresponding interface state.

IPv6 was broken with commit 8c14586fc320 (part of 4.7) and fixed with
a435a07f9164 (part of 4.8).

## Variations

There are various iterations of this lab:

 - 5dfab33b776b will use multiple routing tables and "ip rules" to
   select the right ones. The drawback of this solution is that it
   doesn't scale well due to the huge number of ip rules needed.

 - adcd356527fb is using a DHCP relay instead of a local DHCP server

 - 453ea916aec7 is using namespaces to get multiple routing
   tables. Unfortunately, orchestrators (like libvirt) may not support
   namespaces and moving an interface manually to another namespace
   may confuse them (and the information they provide may become
   inaccurate since there is no unique interface names accross
   namespaces). Moreover, this requires to run several instances of
   BIRD (no support for namespaces in BIRD either).

 - d86e9ed65886 makes use of VRF, a recent (4.3+ for IPv4, 4.4+ for
   IPv6) addition to Linux that provides L3 separation (like a bridge,
   but for L3). We are back to using multiple routing tables, but the
   number of ip rules is now constant (4.8+ can even remove the need
   for specific ip rules). However, the feature is quite recent, has
   poor performance before 4.8 (7889681f4a6c), integration with
   Netfilter is still a moving target. Moreover, the DHCP server isn't
   able to send an answer for some reason (socket not bound to the
   VRF?).

 - d4ad9a028b51 uses firewall marks instead of VRF. Unfortunately, ARP
   proxying breaks as it is not possible to put marks for those
   "internal" requests. Regular reverse path filtering wouldn't work
   either (but we used the one based on Netfilter).

The current iteration uses multiple routing tables and "ip rules". The
scalability issues are avoided by specifying "ip rules" for private
traffic (there should be less of them), local traffic and have a catch
all for what we assume to be public traffic. This only works with
3.15+ kernels. Before that, the kernel was checking the reachability
of the next hop with iif equal to 0 and therefore didn't match any
rule. This was changed in kernel commit 6a662719c986. For a version
compatible with older kernels, have a look at commit 39b804c1e759. It
uses BIRD to make copies of the device routes to the other
tables (need BIRD 1.6.2+).

The hypervisor has a local table dedicated for its own use
(local-out). This table is built with BIRD (need 1.6.2+, otherwise,
use commit 28a0f7758d08). This is both to enable use of several
default routes (one public, one private) and to avoid hyperv to use
the public interface more than necessary (like contact arbitrary
hosts).

It is possible to replace one of the route reflector (RR2) by a
Juniper vRR. You need a proper image (at least 15.1) to be placed in
`images/junos-vrr.img`.

## Use of BFD and graceful restart

There are some limitations of using BFD with BIRD. If the hypervisors
are directly connected to the route reflectors (for example, if the
top of the rack switch act as a route reflector), it makes sense to
disabled BFD: link failure should be enough to detect the peer is
down.

With BIRD, BFD is handled by the same process as BGP. It disables the
ability to use BGP graceful restart since the BFD sessions are
interrupted at the same time than the BGP session. If you disable BFD,
you can enable graceful restart. This can be done by reverting commit
10762d58961c. When disabling BFD, you may want to check that a link
down is enough to bring down the BGP session. Otherwise, you will also
need to reduce the BGP timers.

However, because of the use of route reflectors, it's difficult to use
graceful restart. If a link goes down, the directly connected
hypervisor will detect that and invalidate routes, but the other
hypervisors (or systems) won't see the link down. The route reflector
will keep the route and distribute it. Other systems will happily use
the route even if it's invalid.

## ECMP for IPv4

Starting from kernel 3.6 and until kernel 4.4, ECMP for IPv4 is done
on a round-robin fashion instead of per-flow. It is likely to cause
some out-of-order packets and some flows may even be dropped by some
IDS. Flow-based hashing is back only with 4.4. See a
nice [summary on reddit][1]. When using affected kernels, it is
advisable to disable ECMP when installing routes from BIRD, at least
for the default route.

[1]: https://www.reddit.com/r/networking/comments/4q3wmq/ipv4_flow_based_ecmp_broken_in_linux_kernels_36/

## Anycast

In the lab, 203.0.113.10 and 2001:db8:cb00:7100:5254:33ff:fe00:100 are
anycasted. However, this requires support for RFC 7911 in the route
reflectors. BIRD comes with this support, but on JunOS, the support is
only available from 15.1 and is really effective with 16.1. Moreover,
on a given hypervisor, flows are not anycasted (unless an ECMP route
is installed specifically).

## Overlay network

A very simple overlay network is setup using VXLAN with unicast. It
assumes that no routing is needed (which is the case in our
lab). Otherwise, TTL must be increased.

We should use a higher than normal MTU (at least 1550) for underlying
interfaces to ensure we can put 1500 bytes in virtual
frames. Unfortunately, `vde_switch` doesn't support large
frames. Therefore, we keep the default MTU. This could be changed in
`src/vde_switch/port.h` by increasing from 1504 to something larger in
`struct packet`.

Any other solution can be chosen, notably the use of BGP EVPN. See
`../lab-vxlan` lab for solutions. All of them should work fine on this
topology.

## Reducing the number of BGP sessions

It's important that the BGP session with the route reflector goes down
when the data path is broken. Each hypervisor maintains 8 BGP sessions
(IPv4/IPv6, two possible paths, public/private).

This could be reduced to 4 BGP sessions by using MP-BGP and using one
session for both IPv4 and IPv6. This is not done here since BIRD
cannot do that (until BIRD 2).

This could also be reduced by using a common session for
public/private routes and using communities to put the routes in the
right table. However, a clean separation is something nice too.

It is not advisable to reduce the number of sessions by using only one
of the data path. There are several reasons:

 - next hop is set to self, the other route wouldn't exist at all
   (maybe it would be possible to synthetize it and use add-path to
   announce it)
   
 - if the used data path is broken, the other path is useless as no
   announce will go through it
   
 - if the unused data path is broken, it would still be announced
   through the working one (no way to detect it is broken with an L2
   fabric)
   
However, this would be possible to only have one BGP session
announcing everything by using an internal routing protocol like OSPF,
using a loopback as next hop self, put BGP sessions between the
loopbacks and tunneling data between loopbacks.
