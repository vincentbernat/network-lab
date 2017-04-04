# Simple experiment about a Linux bridge routing

In this lab, we have:

 - Two clients: C1 and C2
 - A Linux bridge: S1
 - Another device: W1
 
The two clients are bridged on S1 using br0. S1 shares some kind of
administrative interface with W1. S1 is expected to act primarily as a
bridge. We expect C1 to be able to speak with C2 but we also expect C1
to be unable to access W1.

The lab shows that one-way communication is still possible using IPv6
or IPv4. If IPv6 was disabled on the bridge, it is still possible to
use the same workaround as for IPv4.

Routing needs to be enabled to access W1. However, even if routing was
disabled, C1 and C2 would be able to access to some administrative
services running on S1.

One-way communication would enable the following attacks:

 - floods
 - covert channels
 - interaction with UDP-related services (like SNMP)

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
