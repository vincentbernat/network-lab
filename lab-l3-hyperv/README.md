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

The current iteration make use of VRF, a recent (4.3+ for IPv4, 4.4+
for IPv6) addition to Linux that provides L3 separation (like a
bridge, but for L3). We are back to using multiple routing tables, but
the number of ip rules is now constant (4.8+ can even remove the need
for specific ip rules). However, the feature is quite recent, has poor
performance before 4.8 (7889681f4a6c), integration with Netfilter is
still a moving target.

Moreover, the DHCP server isn't able to send an answer for some reason
(socket not bound to the VRF?).
