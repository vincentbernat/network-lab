Proxy ARP restrictions on JunOS
===============================

This lab is about a "bug" in how "proxy-arp restricted" is implemented
on JunOS. The lab uses a SRX appliance but the same problem exists on
more recent versions of JunOS and on other platforms (MX).

The problem
-----------

In the initial state, A, B and SRX share a L2 network and can ping each
other directly:

    A# oping -c3 192.0.2.1 192.0.2.2
    PING 192.0.2.1 (192.0.2.1) 56 bytes of data.
    PING 192.0.2.2 (192.0.2.2) 56 bytes of data.
    56 bytes from 192.0.2.1 (192.0.2.1): icmp_seq=1 ttl=64 time=0.39 ms
    56 bytes from 192.0.2.2 (192.0.2.2): icmp_seq=1 ttl=64 time=0.45 ms
    56 bytes from 192.0.2.1 (192.0.2.1): icmp_seq=2 ttl=64 time=0.62 ms
    56 bytes from 192.0.2.2 (192.0.2.2): icmp_seq=2 ttl=64 time=0.34 ms
    56 bytes from 192.0.2.1 (192.0.2.1): icmp_seq=3 ttl=64 time=0.68 ms
    56 bytes from 192.0.2.2 (192.0.2.2): icmp_seq=3 ttl=64 time=0.61 ms
    
    --- 192.0.2.1 ping statistics ---
    3 packets transmitted, 3 received, 0.00% packet loss, time 1.7ms
    RTT[ms]: min = 0, median = 1, p(67) = 1, max = 1
    
    --- 192.0.2.2 ping statistics ---
    3 packets transmitted, 3 received, 0.00% packet loss, time 1.4ms
    RTT[ms]: min = 0, median = 0, p(67) = 0, max = 1
    
    B# oping -c3 192.0.2.1 192.0.2.3
    PING 192.0.2.1 (192.0.2.1) 56 bytes of data.
    PING 192.0.2.3 (192.0.2.3) 56 bytes of data.
    56 bytes from 192.0.2.1 (192.0.2.1): icmp_seq=1 ttl=64 time=0.41 ms
    56 bytes from 192.0.2.3 (192.0.2.3): icmp_seq=1 ttl=64 time=0.49 ms
    56 bytes from 192.0.2.1 (192.0.2.1): icmp_seq=2 ttl=64 time=0.64 ms
    56 bytes from 192.0.2.3 (192.0.2.3): icmp_seq=2 ttl=64 time=0.38 ms
    56 bytes from 192.0.2.1 (192.0.2.1): icmp_seq=3 ttl=64 time=1.58 ms
    56 bytes from 192.0.2.3 (192.0.2.3): icmp_seq=3 ttl=64 time=0.78 ms
    
    --- 192.0.2.1 ping statistics ---
    3 packets transmitted, 3 received, 0.00% packet loss, time 2.6ms
    RTT[ms]: min = 0, median = 1, p(67) = 1, max = 2
    
    --- 192.0.2.3 ping statistics ---
    3 packets transmitted, 3 received, 0.00% packet loss, time 1.6ms
    RTT[ms]: min = 0, median = 0, p(67) = 0, max = 1

Then, we can check the ARP table on SRX:

    root@SRX> show arp no-resolv
    MAC Address       Address         Interface  Flags
    50:54:33:00:00:02 192.0.2.2       ge-0/0/1.0 none
    50:54:33:00:00:01 192.0.2.3       ge-0/0/1.0 none
    50:54:33:00:00:03 198.51.100.0    ge-0/0/2.0 none
    Total entries: 3

Now, we will move the IP of B to the loopback. This will make B
announce the same IP through BGP to SRX.

    B# ip link set down dev eth0
    B# ip route add 192.0.2.2/32 dev lo
    
From here, we can check SRX knows the route to 192.0.2.2:

    root@SRX> show route 192.0.2.2
    
    inet.0: 5 destinations, 5 routes (5 active, 0 holddown, 0 hidden)
    + = Active Route, - = Last Active, * = Both
    
    192.0.2.2/32       *[BGP/170] 00:03:30, localpref 100
                          AS path: I
                        > to 198.51.100.0 via ge-0/0/2.0

Thanks to proxy ARP, we expect A to be able to ping B and be
routed. This is not the case:

    A# oping -c3 192.0.2.2
    PING 192.0.2.2 (192.0.2.2) 56 bytes of data.
    echo reply from 192.0.2.2 (192.0.2.2): icmp_seq=1 timeout
    echo reply from 192.0.2.2 (192.0.2.2): icmp_seq=2 timeout
    echo reply from 192.0.2.2 (192.0.2.2): icmp_seq=3 timeout
    
    --- 192.0.2.2 ping statistics ---
    3 packets transmitted, 0 received, 100.00% packet loss, time 0.0ms

If we flush the ARP table (or wait a minute for the entry to
disappear), we can get an answer:

    root@SRX> clear arp
    192.0.2.2        deleted
    192.0.2.3        deleted
    
    root@SRX> show arp no-resolve
    MAC Address       Address         Interface     Flags
    50:54:33:00:00:03 198.51.100.0    ge-0/0/2.0           none
    Total entries: 1
    
    A# oping -c3 192.0.2.2
    PING 192.0.2.2 (192.0.2.2) 56 bytes of data.
    56 bytes from 192.0.2.2 (192.0.2.2): icmp_seq=1 ttl=63 time=1.03 ms
    56 bytes from 192.0.2.2 (192.0.2.2): icmp_seq=2 ttl=63 time=1.51 ms
    56 bytes from 192.0.2.2 (192.0.2.2): icmp_seq=3 ttl=63 time=1.47 ms
    
    --- 192.0.2.2 ping statistics ---
    3 packets transmitted, 3 received, 0.00% packet loss, time 4.0ms
    RTT[ms]: min = 1, median = 1, p(67) = 1, max = 2
    
    A# ip neigh show
    192.0.2.2 dev eth0 lladdr 50:54:33:00:00:04 STALE
    192.0.2.1 dev eth0 lladdr 50:54:33:00:00:04 STALE

Now, if we get the other way around, it works just fine because the
SRX can route to the destination:

    B# ip route del 192.0.2.2/32 dev lo
    B# ip link set up dev lo
    
    A# oping -c3 192.0.2.2
    PING 192.0.2.2 (192.0.2.2) 56 bytes of data.
    56 bytes from 192.0.2.2 (192.0.2.2): icmp_seq=1 ttl=64 time=1.25 ms
    56 bytes from 192.0.2.2 (192.0.2.2): icmp_seq=2 ttl=64 time=0.46 ms
    56 bytes from 192.0.2.2 (192.0.2.2): icmp_seq=3 ttl=64 time=0.40 ms
    
    --- 192.0.2.2 ping statistics ---
    3 packets transmitted, 3 received, 0.00% packet loss, time 2.1ms
    RTT[ms]: min = 0, median = 0, p(67) = 0, max = 1
    A# ip neigh show
    192.0.2.2 dev eth0 lladdr 50:54:33:00:00:04 REACHABLE
    192.0.2.1 dev eth0 lladdr 50:54:33:00:00:04 STALE

The SRX also sent ICMP redirects that were quickly understood by
A. After a few seconds:

    A# ip neigh show
    192.0.2.1 dev eth0 lladdr 50:54:33:00:00:04 REACHABLE
    192.0.2.2 dev eth0 lladdr 50:54:33:00:00:02 STALE

Also, another odd behavior. If we want to delete the ARP entry before
its expiration, it doesn't work. No error, but entry is still here:

    root@SRX> show arp no-resolve expiration-time
    MAC Address       Address         Interface     Flags    TTE
    50:54:33:00:00:02 192.0.2.2       ge-0/0/1.0           none  206
    50:54:33:00:00:01 192.0.2.3       ge-0/0/1.0           none  104
    50:54:33:00:00:03 198.51.100.0    ge-0/0/2.0           none  139
    Total entries: 3
    
    root@SRX> clear arp hostname 192.0.2.2 interface ge-0/0/1.0
    
    root@SRX> show route 192.0.2.2
    
    inet.0: 5 destinations, 5 routes (5 active, 0 holddown, 0 hidden)
    + = Active Route, - = Last Active, * = Both
    
    192.0.2.2/32       *[BGP/170] 00:00:09, localpref 100
                          AS path: I
                        > to 198.51.100.0 via ge-0/0/2.0

As long as we have a route overlapping the ARP entry, we cannot delete
the entry. If we remove the route, there is no problem to remove the entry:

    B# ip route del 192.0.2.2/32 dev lo
    
    root@SRX> clear arp hostname 192.0.2.2 interface ge-0/0/1.0
    192.0.2.2        deleted

Workarounds
-----------

A limited workaround is to lower the aging timer. This is done in the
lab and the timer is 1 minute. This is not really satisfying.

Another workaround would be to use event scripts. However, since we
cannot delete the ARP entry, this won't work.
