# IPsec VPN, VTI interfaces and PMTUD

## Without specific IP rules

From C1:

    $ ping -c3 172.17.1.10
    PING 172.17.1.10 (172.17.1.10) 56(84) bytes of data.
    64 bytes from 172.17.1.10: icmp_seq=1 ttl=62 time=0.831 ms
    64 bytes from 172.17.1.10: icmp_seq=2 ttl=62 time=1.56 ms
    64 bytes from 172.17.1.10: icmp_seq=3 ttl=62 time=1.40 ms
    
    --- 172.17.1.10 ping statistics ---
    3 packets transmitted, 3 received, 0% packet loss, time 5ms
    rtt min/avg/max/mdev = 0.831/1.261/1.557/0.312 ms
    $ tracepath 172.17.1.10
     1?: [LOCALHOST]                      pmtu 1500
     1:  172.16.1.1                                            0.238ms
     1:  172.16.1.1                                            0.125ms
     2:  172.16.1.1                                            0.141ms pmtu 1426
     2:  172.16.1.1                                            0.752ms pmtu 1406
     2:  169.254.15.1                                          1.596ms
     3:  169.254.15.1                                          1.337ms pmtu 1320
     3:  172.17.1.10                                           0.665ms reached
         Resume: pmtu 1320 hops 3 back 3
    $ ip route get 172.17.1.10
    172.17.1.10 via 172.16.1.1 dev eth0 src 172.16.1.10 uid 0
        cache expires 584sec mtu 1320

This works. On V1, MTU for 203.0.113.19 is updated when receiving the
"need fragment" ICMP message. Here, when external MTU is 1480, we get
1426 for internal MTU. When external MTU later drops to 1460, we
get 1406. We finally gets 1320 because of a bottleneck after the VPN
(in the meantime, this latest bottleneck was removed from `./setup` as
we don't care much about it).

## With a specific IP rule

If the VPN servers have a more complex setup with IP rules, I would
have expected this to not work as `ipv4_update_pmtu()` isn't provided
with an output interface to work with. However, it works too...
