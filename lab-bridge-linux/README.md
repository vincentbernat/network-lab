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

## MACVTAP/MACVLAN

The situation is similar with MACVTAP/MACVLAN interfaces but this is a
bit more complex as the setup is not symmetric: packets from the
MACVTAP interfaces cannot go into the host IP stack. However, the
lower interface can still process IP despite the configured "taps". In
the lab, from `ER1`, you can:

    # ip  route add 192.168.14.3/32 dev eth0
    # ping -c3 192.168.14.3
    PING 192.168.14.3 (192.168.14.3) 56(84) bytes of data.
    64 bytes from 192.168.14.3: icmp_seq=1 ttl=63 time=0.332 ms
    64 bytes from 192.168.14.3: icmp_seq=2 ttl=63 time=0.623 ms
    64 bytes from 192.168.14.3: icmp_seq=3 ttl=63 time=0.659 ms
    
    --- 192.168.14.3 ping statistics ---
    3 packets transmitted, 3 received, 0% packet loss, time 2006ms
    rtt min/avg/max/mdev = 0.332/0.538/0.659/0.146 ms

## Restricted bridge

This is a bridge that should work in the same way as a MACVTAP/MACVLAN
interface (no learning) with known isolation solutions.
