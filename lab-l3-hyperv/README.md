# L3 routing on the hypervisor

The goal of this lab is to demonstrate how an hypervisor could be
turned into a router while the VM still think they share some L2
subnet.

The hypervisors distribute routes to VM using BGP (through a pair of
route reflectors) on two distinct L2 layers. ARP proxying is used to
let the hypervisors answer to ARP requests for VM on other hypervisors

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
tables. However, a modified version of BIRD is needed.

It is possible to replace one of the route reflector (RR2) by a
Juniper vRR. You need a proper image (at least 15.1) to be placed in
`images/junos-vrr.img`.
