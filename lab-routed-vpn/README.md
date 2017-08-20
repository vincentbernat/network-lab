# Route-based VPN with Linux

Route-based VPN are easier to handle than policy-based VPN in several cases:

 - several VPN serving the same networks
 - route-learning through BGP

This lab shows both scenarios. The most important point are shown in
the diagram. Notably, each VPN has one public IP address (203.0.113.X)
and can reach a private network (192.168.1.X/26). The goal is to setup
the VPN such that each private network can be routed to another site.

With policy-based VPN, we could have paired the VPNs (V1-1 connects to
V2-1 and V3-1) and statically establish policy for private
networks. However, If both V1-1 and V2-2 are out of order, AS65001
cannot speak to AS65002 anymore. Moreover, policies need to be
declared in addition to routing rules.

With route-based VPN, we can establish a mesh between VPNs and
distribute routes with BGP.

You can test the result from `Rx`:

    $ ip netns exec R1 ping -c2 -I 192.168.1.1 192.168.1.129
    PING 192.168.1.129 (192.168.1.129) from 192.168.1.1 : 56(84) bytes of data.
    64 bytes from 192.168.1.129: icmp_seq=1 ttl=62 time=1.02 ms
    64 bytes from 192.168.1.129: icmp_seq=2 ttl=62 time=3.57 ms
    
    --- 192.168.1.129 ping statistics ---
    2 packets transmitted, 2 received, 0% packet loss, time 1001ms
    rtt min/avg/max/mdev = 1.028/2.300/3.573/1.273 ms

The use of `-I` is needed because the interconnection between Rx and
Vx-y is not announced in the VPN. This lab is a bit messy because the
VPN are also acting as edge routers and interco are not announced to
keep the routing tables small. Therefore, some care needs to be taken
to ping the various IPs.

## Troubleshooting

The internet router should see the various sites:

    $ birdc show route
    BIRD 1.6.3 ready.
    0.0.0.0/0          unreachable [DEFAULT 20:16:23] * (200)
    203.0.113.2/32     via 198.51.100.1 on eth0 [V1_1 20:16:25] * (100) [AS65001i]
                       via 198.51.100.3 on eth0 [V1_2 20:16:26] (100) [AS65001i]
    203.0.113.66/32    via 198.51.100.5 on eth0 [V2_1 20:16:26] * (100) [AS65002i]
                       via 198.51.100.7 on eth0 [V2_2 20:16:27] (100) [AS65002i]
    203.0.113.130/32   via 198.51.100.9 on eth0 [V3_1 20:16:27] * (100) [AS65003i]
                       via 198.51.100.11 on eth0 [V3_2 20:16:28] (100) [AS65003i]
    203.0.113.0/26     via 198.51.100.1 on eth0 [V1_1 20:16:25] * (100) [AS65001i]
                       via 198.51.100.3 on eth0 [V1_2 20:16:26] (100) [AS65001i]
    203.0.113.64/26    via 198.51.100.5 on eth0 [V2_1 20:16:26] * (100) [AS65002i]
                       via 198.51.100.7 on eth0 [V2_2 20:16:27] (100) [AS65002i]
    203.0.113.128/26   via 198.51.100.9 on eth0 [V3_1 20:16:27] * (100) [AS65003i]
                       via 198.51.100.11 on eth0 [V3_2 20:16:28] (100) [AS65003i]
    203.0.113.1/32     via 198.51.100.1 on eth0 [V1_1 20:16:25] * (100) [AS65001i]
                       via 198.51.100.3 on eth0 [V1_2 20:16:26] (100) [AS65001i]
    203.0.113.65/32    via 198.51.100.5 on eth0 [V2_1 20:16:26] * (100) [AS65002i]
                       via 198.51.100.7 on eth0 [V2_2 20:16:27] (100) [AS65002i]
    203.0.113.129/32   via 198.51.100.9 on eth0 [V3_1 20:16:27] * (100) [AS65003i]
                       via 198.51.100.11 on eth0 [V3_2 20:16:28] (100) [AS65003i]

Each VPN should see the `192.168.1.x/26` network through OSPF:

    $ birdc show route protocol INTERNAL
    BIRD 1.6.3 ready.
    192.168.1.0/26     via 172.16.1.1 on eth2 [INTERNAL 20:17:09] * I (150/20) [0.0.0.1]
    172.16.1.0/29      dev eth2 [INTERNAL 20:17:03] I (150/10) [0.0.1.2]

It should also learn a default for Internet:

    $ birdc show route protocol INTERNET
    BIRD 1.6.3 ready.
    0.0.0.0/0          via 198.51.100.0 on eth0 [INTERNET 20:16:26] * (100) [AS65000i]

This default is also known from the Rx routers:

    $ birdc -s /var/run/bird.R1.ctl show route 0.0.0.0/0
    BIRD 1.6.3 ready.
    0.0.0.0/0          multipath [INTERNAL 20:17:11] * E2 (150/10/10000) [0.0.1.2]
            via 172.16.1.2 on eth0 weight 1
            via 172.16.1.3 on eth0 weight 1

Each VPN should be able to ping the other VPN:

    $ fping -S 203.0.113.{1,2,65,66,129,130}
    203.0.113.2 is alive
    203.0.113.65 is alive
    203.0.113.66 is alive
    203.0.113.129 is alive
    203.0.113.130 is alive

IPsec sessions should be up (automatically due to BGP sessions pushing):

    $ ipsec status
    Routed Connections:
            V3-2{4}:  ROUTED, TUNNEL, reqid 4
            V3-2{4}:   0.0.0.0/0 === 0.0.0.0/0
            V3-1{3}:  ROUTED, TUNNEL, reqid 3
            V3-1{3}:   0.0.0.0/0 === 0.0.0.0/0
            V2-2{2}:  ROUTED, TUNNEL, reqid 2
            V2-2{2}:   0.0.0.0/0 === 0.0.0.0/0
            V2-1{1}:  ROUTED, TUNNEL, reqid 1
            V2-1{1}:   0.0.0.0/0 === 0.0.0.0/0
    Security Associations (4 up, 0 connecting):
            V3-1[8]: ESTABLISHED 15 minutes ago, 203.0.113.1[203.0.113.1]...203.0.113.129[203.0.113.129]
            V3-1{12}:  INSTALLED, TUNNEL, reqid 3, ESP SPIs: c45a2736_i ce9393b2_o
            V3-1{12}:   0.0.0.0/0 === 0.0.0.0/0
            V3-2[4]: ESTABLISHED 15 minutes ago, 203.0.113.1[203.0.113.1]...203.0.113.130[203.0.113.130]
            V3-2{10}:  INSTALLED, TUNNEL, reqid 4, ESP SPIs: cd093bab_i c3c0a670_o
            V3-2{10}:   0.0.0.0/0 === 0.0.0.0/0
            V2-1[1]: ESTABLISHED 15 minutes ago, 203.0.113.1[203.0.113.1]...203.0.113.65[203.0.113.65]
            V2-1{11}:  INSTALLED, TUNNEL, reqid 1, ESP SPIs: c8fc7e82_i c564f3af_o
            V2-1{11}:   0.0.0.0/0 === 0.0.0.0/0
            V2-2[2]: ESTABLISHED 15 minutes ago, 203.0.113.1[203.0.113.1]...203.0.113.66[203.0.113.66]
            V2-2{9}:  INSTALLED, TUNNEL, reqid 2, ESP SPIs: c6a70f64_i cf0db1f2_o
            V2-2{9}:   0.0.0.0/0 === 0.0.0.0/0

Each interco should be pingable using the tunnel:

    $ fping 172.22.15.{19,21,23,25}
    172.22.15.19 is alive
    172.22.15.21 is alive
    172.22.15.23 is alive
    172.22.15.25 is alive

BGP sessions should be up:

    $ birdc show proto | grep IBGP_
    IBGP_V2_1 BGP      master   up     20:16:31    Established
    IBGP_V2_2 BGP      master   up     20:16:31    Established
    IBGP_V3_1 BGP      master   up     20:16:31    Established
    IBGP_V3_2 BGP      master   up     20:16:29    Established

And all private subnets should be learnt:

    $ birdc show route table private | grep IBGP_V
    192.168.1.128/26   via 172.22.15.23 on vti5 [IBGP_V3_1 20:17:10] * (100/0) [i]
                       via 172.22.15.25 on vti6 [IBGP_V3_2 20:17:10] (100/0) [i]
    192.168.1.64/26    via 172.22.15.19 on vti3 [IBGP_V2_1 20:17:10] * (100/0) [i]
                       via 172.22.15.21 on vti4 [IBGP_V2_2 20:17:10] (100/0) [i]

## MTU

Another crucial aspect of VPN is MTU. If the outside MTU is 1500, the
inside MTU is reduced by some value. Unfortunately, IPsec doesn't have
a fixed overhead. It depends on the encapsulation and on the
negociated ciphers. Cisco used to have a handy calculator available
but it has behind a login screen.

The best tool is to hope PMTU discovery works. When creating VTI
tunnel, Linux sets the MTU of the interface to 1332 (`ip_tunnel.c`
contains the logic). We can however set it to 1500 and let the outer
layer do its job:

    $ ip netns exec R1 ping -M do -s 1472 -c2 -I 192.168.1.1 192.168.1.129
    PING 192.168.1.129 (192.168.1.129) from 192.168.1.1 : 1472(1500) bytes of data.
    From 172.16.1.3 icmp_seq=1 Frag needed and DF set (mtu = 1438)
    From 172.16.1.3 icmp_seq=2 Frag needed and DF set (mtu = 1438)
    
    --- 192.168.1.129 ping statistics ---
    2 packets transmitted, 0 received, +2 errors, 100% packet loss, time 1005ms
    
    $ ip netns exec R1 ip route get 192.168.1.129
    192.168.1.129 via 172.16.1.2 dev eth0  src 172.16.1.1
        cache  expires 590sec mtu 1438

This is with AES128+SHA256 using transport mode.

As long as PMTU works, no workaround is needed. However, you might
want to use TCP MSS clamping to avoid problems with broken equipments
(notably if the MTU is lowered on the encapsulated path, the VPN has
to handle PTMU discovery itself to update the MTU to the destination
and trigger another PMTU exception for the source). This would also
potentially hide a misconfiguration. With a MTU of 1438, TCP MSS is
1398.
