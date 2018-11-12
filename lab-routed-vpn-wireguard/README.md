# Route-based VPN with Linux

This is similar to `lab-routed-vpn` but it uses
[WireGuard](https://www.wireguard.com/) instead of StrongSwan.

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

We also support transport IPv4 on top of IPv6. To test this aspect,
use:

    $ ip netns exec R1 ping -c2 10.0.3.1

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

Handshakes should have been successful:

    $ ip netns exec private wg
    interface: wg3
    public key: apoMf/5YLR1i8TEYidTOhLRFHuFBS1ls/E9KR/ixRlg=
    private key: (hidden)
    listening port: 5803
    
    peer: Jixsag44W8CFkKCIvlLSZF86/Q/4BovkpqdB9Vps5Sk=
      endpoint: [2001:db8:2::1]:5801
      allowed ips: 0.0.0.0/0, ::/0
      latest handshake: 6 seconds ago
      transfer: 47.38 KiB received, 49.27 KiB sent
    
    interface: wg4
      public key: apoMf/5YLR1i8TEYidTOhLRFHuFBS1ls/E9KR/ixRlg=
      private key: (hidden)
      listening port: 5804
    
    peer: gUYqcT+GFlwaKXHfgAbNBUWSrT0MJL8KoCpHnfo7fxg=
      endpoint: [2001:db8:2::2]:5801
      allowed ips: 0.0.0.0/0, ::/0
      latest handshake: 20 seconds ago
      transfer: 5.15 KiB received, 12.13 KiB sent
    
    interface: wg5
      public key: apoMf/5YLR1i8TEYidTOhLRFHuFBS1ls/E9KR/ixRlg=
      private key: (hidden)
      listening port: 5805
    
    peer: trTzZeGwfSJaCDr7pM39VLeZCBqoxCrRJYuslIkEJDw=
      endpoint: [2001:db8:3::1]:5801
      allowed ips: 0.0.0.0/0, ::/0
      latest handshake: 17 seconds ago
      transfer: 4.85 KiB received, 11.82 KiB sent
    
    interface: wg6
      public key: apoMf/5YLR1i8TEYidTOhLRFHuFBS1ls/E9KR/ixRlg=
      private key: (hidden)
      listening port: 5806
    
    peer: 1p4DBrC/tgA3UEx55fQYzJITOEwBNQzKu6jrlNVzUQU=
      endpoint: [2001:db8:3::2]:5801
      allowed ips: 0.0.0.0/0, ::/0
      latest handshake: 16 seconds ago
      transfer: 4.61 KiB received, 11.70 KiB sent

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
inside MTU is reduced by some value. With contrast to IPsec, it's easy
to get the right MTU (1420), so we leave the system works with that.
