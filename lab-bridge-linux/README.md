# Simple experiment about a Linux bridge routing

In this lab, we have:

 - 3 VM
 - 1 HV with a Linux bridge to a public network, other NIC is connected to a private network
 
## Workarounds

 - Disabling IPv6 on br0 (`net.ipv6.conf.br0.disable_ipv6`). Not
   possible with IPv4.

 - Enabling RP filtering on the bridge (`net.ipv4.conf.br0.rp_filter`,
   `iptables -t raw -I PREROUTING -i br0 -m rpfilter -j ACCEPT`).

 - Using firewall rules to ensure only frames bridged are forwarded
   (`iptables -I FORWARD -m physdev ! --physdev-is-bridged -i br0 -j DROP`)
   and no local delivery is allowed (`iptables -I INPUT -i br0 -j
   DROP`).
   
 - Alternatively disable `net.bridge.bridge-nf-call-iptables` and drop
   any IP traffic. Bridged traffic won't go through Netfilter
   (`iptables -t raw -I PREROUTING -i br0 -j DROP`).

 - Additionally, ARP traffic can be disabled with `ip link set arp off
   dev br0` (however, without an IP on the interface, Linux won't send
   an answer).

More information available in this [blog post][].

[blog post]: https://vincent.bernat.im/en/blog/2017-linux-bridge-isolation
