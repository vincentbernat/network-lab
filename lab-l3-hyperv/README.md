# L3 routing on the hypervisor

The goal of this lab is to demonstrate how an hypervisor could be
turned into a router while the VM still think they share some L2
subnet.

The hypervisors distribute routes to VM using BGP (through a pair of
route reflectors) on two distinct L2 layers. ARP proxying is used to
let the hypervisors answer to ARP requests for VM on other hypervisors

This lab is also compatible with IPv6 but there are two drawbacks:

 - BIRD doesn't support correctly IPv6 ECMP routes. Therefore, only
   one route gets installed.

 - NDP proxying in Linux requires the declaration of all IP that
   should be proxied. To avoid that, a userland proxy (ndppd) is
   used. Another option would be to program those IP using some daemon
   listening to netlink messages for added/removed routes.

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
all for what we assume to be public traffic. To be compatible with
kernels before 3.15, BIRD also copy the direct routes from the main
table to the public table. This is needed before commit
6a662719c9868b3d6c7d26b3a085f0cd3cc15e64 as the kernel doesn't use
"iif lo" for internal lookups. Since the kernel won't accept anything
else than a "scope link" route, this needs a patched version of
BIRD. Patch is available here:

 http://bird.network.cz/pipermail/bird-users/2016-September/010593.html
