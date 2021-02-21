# LACP over a Linux bridge

This is an experiment on forwarding LACP frames over a Linux bridge.
This is disabled by default but can be enabled with `group_fwd_mask`.
This cannot be done per-bridge (arbitrary restriction) but can be done
per-port since 4.15.

```
H2$ ping fe80::5254:33ff:fe00:1 -I bond0 -c 3
ping: Warning: source address might be selected on device other than: bond0
PING fe80::5254:33ff:fe00:1(fe80::5254:33ff:fe00:1) from :: bond0: 56 data bytes
64 bytes from fe80::5254:33ff:fe00:1%bond0: icmp_seq=1 ttl=64 time=0.370 ms
64 bytes from fe80::5254:33ff:fe00:1%bond0: icmp_seq=2 ttl=64 time=1.92 ms
64 bytes from fe80::5254:33ff:fe00:1%bond0: icmp_seq=3 ttl=64 time=1.91 ms

--- fe80::5254:33ff:fe00:1 ping statistics ---
3 packets transmitted, 3 received, 0% packet loss, time 2003ms
rtt min/avg/max/mdev = 0.370/1.399/1.921/0.727 ms
```

```
H2$ cat /proc/net/bonding/bond0
Ethernet Channel Bonding Driver: v5.10.0-3-amd64

Bonding Mode: IEEE 802.3ad Dynamic link aggregation
Transmit Hash Policy: layer2 (0)
MII Status: up
MII Polling Interval (ms): 100
Up Delay (ms): 0
Down Delay (ms): 0
Peer Notification Delay (ms): 0

802.3ad info
LACP rate: fast
Min links: 0
Aggregator selection policy (ad_select): stable
System priority: 65535
System MAC address: 50:54:33:00:00:07
Active Aggregator Info:
        Aggregator ID: 1
        Number of ports: 2
        Actor Key: 9
        Partner Key: 9
        Partner Mac Address: 50:54:33:00:00:01

Slave Interface: eth0
MII Status: up
Speed: 1000 Mbps
Duplex: full
Link Failure Count: 0
Permanent HW addr: 50:54:33:00:00:07
Slave queue ID: 0
Aggregator ID: 1
Actor Churn State: none
Partner Churn State: none
Actor Churned Count: 0
Partner Churned Count: 0
details actor lacp pdu:
    system priority: 65535
    system mac address: 50:54:33:00:00:07
    port key: 9
    port priority: 255
    port number: 1
    port state: 63
details partner lacp pdu:
    system priority: 65535
    system mac address: 50:54:33:00:00:01
    oper key: 9
    port priority: 255
    port number: 1
    port state: 63

Slave Interface: eth1
MII Status: up
Speed: 1000 Mbps
Duplex: full
Link Failure Count: 0
Permanent HW addr: 50:54:33:00:00:08
Slave queue ID: 0
Aggregator ID: 1
Actor Churn State: none
Partner Churn State: none
Actor Churned Count: 0
Partner Churned Count: 0
details actor lacp pdu:
    system priority: 65535
    system mac address: 50:54:33:00:00:07
    port key: 9
    port priority: 255
    port number: 2
    port state: 63
details partner lacp pdu:
    system priority: 65535
    system mac address: 50:54:33:00:00:01
    oper key: 9
    port priority: 255
    port number: 2
    port state: 63
```

## Links

 - https://github.com/arista-netdevops-community/kvm-lab-for-network-engineers
 - https://lists.openwall.net/netdev/2018/10/01/128
 - https://lists.openwall.net/netdev/2018/10/02/168
 - https://github.com/torvalds/linux/commit/5af48b59f35c (present in 4.15)
 - https://twitter.com/ntdvps/status/1363427163341471750
 - https://netdevops.me/2021/transparently-redirecting-packets/frames-between-interfaces/
