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
