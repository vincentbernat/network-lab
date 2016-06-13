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
   select the right ones.
 - adcd356527fb is using a DHCP relay instead of a local DHCP server
