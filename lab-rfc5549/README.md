# RFC 5549 tests

On Linux:

```console
$ ip route show
203.0.113.12 via inet6 2001:db8:1::12 dev eth0
$ ip route get 203.0.113.12
203.0.113.12 via inet6 2001:db8:1::12 dev eth0 src 203.0.113.11 uid 0
    cache
$ ping -c 2 203.0.113.12
PING 203.0.113.12 (203.0.113.12) 56(84) bytes of data.
64 bytes from 203.0.113.12: icmp_seq=1 ttl=64 time=0.252 ms
64 bytes from 203.0.113.12: icmp_seq=2 ttl=64 time=0.926 ms

--- 203.0.113.12 ping statistics ---
2 packets transmitted, 2 received, 0% packet loss, time 1001ms
rtt min/avg/max/mdev = 0.252/0.589/0.926/0.337 ms
```

On the other side:

```console
$ tcpdump -pni eth0 -c 2
tcpdump: verbose output suppressed, use -v or -vv for full protocol decode
listening on eth0, link-type EN10MB (Ethernet), capture size 262144 bytes
10:55:03.488038 IP 203.0.113.11 > 203.0.113.12: ICMP echo request, id 425, seq 1, length 64
10:55:03.488066 IP 203.0.113.12 > 203.0.113.11: ICMP echo reply, id 425, seq 1, length 64
2 packets captured
2 packets received by filter
0 packets dropped by kernel
```
