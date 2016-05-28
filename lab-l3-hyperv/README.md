# L3 routing on the hypervisor

The goal of this lab is to demonstrate how an hypervisor could be
turned into a router while the VM still think they share some L2
subnet.

The hypervisors distribute routes to VM using BGP (through a pair of
route reflectors) on two distinct L2 layers. ARP proxying is used to
let the hypervisors answer to ARP requests for VM on other hypervisors

This lab is also compatible with IPv6 but there are two drawbacks:

 - BIRD doesn't support correctly IPv6 ECMP routes. Sometimes, two
   routes are installed, sometimes only one.

 - NDP proxying in Linux requires the declaration of all IP that
   should be proxied. To avoid that, a userland proxy (ndppd) is
   used. Another option would be to program those IP using some daemon
   listening to netlink messages for added/removed routes.

Also, the IPv4 version is using one BIRD and one GoBGP as route
reflectors. GoBGP is not the most flexible tool for the job as it only
supports one RIB (except in route server mode). For IPv6, only BIRD is
used.
