# Route-based VPN with Linux

Route-based VPN are easier to handle than policy-based VPN in several cases:

 - several VPN serving the same networks
 - route-learning through BGP

This lab shows both scenarios. The most important point are shown in
the diagram. Notably, each VPN has one public IP address (2001:db8:X::Y)
and can reach a private network (2001:db8:aX::/64). The goal is to setup
the VPN such that each private network can be routed to another site.

With policy-based VPN, we could have paired the VPNs (V1-1 connects to
V2-1 and V3-1) and statically establish policy for private
networks. However, If both V1-1 and V2-2 are out of order, AS65001
cannot speak to AS65002 anymore. Moreover, policies need to be
declared in addition to routing rules.

With route-based VPN, we can establish a mesh between VPNs and
distribute routes with BGP.

You can test the result from `Rx`:

    $ ip netns exec R1 ping -c2 2001:db8:a3::1
    PING 2001:db8:a3::1(2001:db8:a3::1) 56 data bytes
    64 bytes from 2001:db8:a3::1: icmp_seq=1 ttl=62 time=1.43 ms
    64 bytes from 2001:db8:a3::1: icmp_seq=2 ttl=62 time=2.43 ms
    
    --- 2001:db8:a3::1 ping statistics ---
    2 packets transmitted, 2 received, 0% packet loss, time 1001ms
    rtt min/avg/max/mdev = 1.434/1.932/2.430/0.498 ms

The VPN hosts are splitted in two namespaces: the main namespace
provides the public connectivity. It is not connected to the Rx
nodes. The private namespace is managing the private connectivity and
is not connected to "Internet". The only way for a packet to goes from
one namespace to another is to be encapsulated into an IPsec policy.

The main namespace is using OSPFv3, but this doesn't really
matter. This is just a way to let each VPN see each others.

For a similar lab using IPv4, see commit 82d2cc0e4a33.

## Troubleshooting

Each VPN should see the `2001:db8:aX::/64` network through OSPF:

    $ birdc6 -s /run/bird6.private.ctl show route protocol PRIVATE
    BIRD 1.6.3 ready.
    2001:db8:a1::/64   via fe80::5254:33ff:fe00:13 on eth2 [PRIVATE 19:51:17] * I (150/20) [0.0.0.1]

Each VPN should be able to ping the other VPN:

    $ fping -S 2001:db8:{1,2,3}::{1,2}
    2001:db8:1::2 is alive
    2001:db8:2::1 is alive
    2001:db8:2::2 is alive
    2001:db8:3::1 is alive
    2001:db8:3::2 is alive

IPsec sessions should be up:

    $ ipsec status
    Security Associations (4 up, 0 connecting):
            V2-2[8]: ESTABLISHED 9 minutes ago, 2001:db8:1::1[2001:db8:1::1]...2001:db8:2::2[2001:db8:2::2]
            V2-2{7}:  INSTALLED, TUNNEL, reqid 2, ESP SPIs: cb08fed4_i c39e2f0b_o
            V2-2{7}:   ::/0 === ::/0
            V3-2[7]: ESTABLISHED 9 minutes ago, 2001:db8:1::1[2001:db8:1::1]...2001:db8:3::2[2001:db8:3::2]
            V3-2{6}:  INSTALLED, TUNNEL, reqid 1, ESP SPIs: cdb78529_i c30ebb59_o
            V3-2{6}:   ::/0 === ::/0
            V2-1[5]: ESTABLISHED 9 minutes ago, 2001:db8:1::1[2001:db8:1::1]...2001:db8:2::1[2001:db8:2::1]
            V2-1{4}:  INSTALLED, TUNNEL, reqid 3, ESP SPIs: cc64a328_i c6fe7aa1_o
            V2-1{4}:   ::/0 === ::/0
            V3-1[3]: ESTABLISHED 9 minutes ago, 2001:db8:1::1[2001:db8:1::1]...2001:db8:3::1[2001:db8:3::1]
            V3-1{8}:  INSTALLED, TUNNEL, reqid 4, ESP SPIs: c6510832_i cff1ad0b_o
            V3-1{8}:   ::/0 === ::/0

Each interco should be pingable using the tunnel:

    $ ip netns exec private fping 2001:db8:ff::{1,3,5,7}
    2001:db8:ff::1 is alive
    2001:db8:ff::3 is alive
    2001:db8:ff::5 is alive
    2001:db8:ff::7 is alive

BGP sessions should be up:

    $ birdc6 -s /run/bird6.private.ctl show proto | grep IBGP_
    IBGP_V2_1 BGP      master   up     20:16:31    Established
    IBGP_V2_2 BGP      master   up     20:16:31    Established
    IBGP_V3_1 BGP      master   up     20:16:31    Established
    IBGP_V3_2 BGP      master   up     20:16:29    Established

And all private subnets should be learnt:

    $ birdc6 -s /run/bird6.private.ctl show route | grep IBGP_V
    2001:db8:a2::/64   via 2001:db8:ff::1 on vti3 [IBGP_V2_1 13:23:43] ! (160) [AS65002i]
                       via 2001:db8:ff::3 on vti4 [IBGP_V2_2 13:23:34] (160) [AS65002i]
    2001:db8:a3::/64   via 2001:db8:ff::5 on vti6 [IBGP_V3_1 13:23:39] ! (160) [AS65003i]
                       via 2001:db8:ff::7 on vti4 [IBGP_V3_2 13:23:39] (160) [AS65002i]

The exclamation mark is because BIRD isn't able to handle IPv6 ECMP
routes correctly yet with recent kernels (4.11+).

## MTU

Another crucial aspect of VPN is MTU. If the outside MTU is 1500, the
inside MTU is reduced by some value. Unfortunately, IPsec doesn't have
a fixed overhead. It depends on the encapsulation and on the
negociated ciphers. Cisco used to have a handy calculator available
but it has behind a login screen.

The best tool is to hope PMTU discovery works. When creating VTI
tunnel, Linux sets the MTU of the interface to 1332 (`ip_tunnel.c`
contains the logic) which is too low. It appears this is a bug fixed
in a32452366b72 for IPv4 and c6741fbed6dc for IPv6. We can however set
it to 1500 and let the outer layer do its job:

    $ ip netns exec R1 ping -M do -s 1452 -c2 2001:db8:a3::1
    PING 2001:db8:a3::1(2001:db8:a3::1) 1452 data bytes
    From 2001:db8:ff::8 icmp_seq=2 Packet too big: mtu=1406
    
    --- 2001:db8:a3::1 ping statistics ---
    2 packets transmitted, 0 received, +1 errors, 100% packet loss, time 1021ms
    
    $ ip netns exec R1 ip route get 2001:db8:a3::1
    2001:db8:a3::1 from :: via fe80::5254:33ff:fe00:3 dev eth0 src 2001:db8:a1::1 metric 0
        cache  expires 559sec mtu 1406 pref medium

This is with AES128+SHA256 using transport mode. With
AES256-GCM16+ECP384, the MTU is increased to 1426.

As long as PMTU works, no workaround is needed. However, you might
want to use TCP MSS clamping to avoid problems with broken equipments
(notably if the MTU is lowered on the encapsulated path, the VPN has
to handle PTMU discovery itself to update the MTU to the destination
and trigger another PMTU exception for the source). This would also
potentially hide a misconfiguration. With a MTU of 1406, TCP MSS is
1366.

## Mark

The VTI interface alter the firewall mark of the packet and there is
no way to set a mask to avoid altering part of the existing
mark. However, this is not a problem as the mark is set only to lookup
the appropriate XFRM policy and then restored:

    u32 orig_mark = skb->mark;
    skb->mark = be32_to_cpu(t->parms.i_key);
    ret = xfrm_policy_check(NULL, XFRM_POLICY_IN, skb, family);
    skb->mark = orig_mark;

This can be checked with Netfilter:

    sysctl -qw net.netfilter.nf_log_all_netns=1
    ip netns exec private ip6tables -t raw -A PREROUTING -j TRACE
    ip netns exec private ip6tables -t raw -A PREROUTING -j TRACE
    ip6tables -t raw -A PREROUTING -j TRACE
    ip6tables -t raw -A PREROUTING -j TRACE

No packet has a mark. However, XFRM policies take a mask because the
mark can be set from something else (notably a Netfilter rule).
