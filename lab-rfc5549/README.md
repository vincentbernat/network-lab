# RFC 5549 tests

On `ping`:

```console
$ ip netns exec eth0 ping -c3 192.0.2.6
PING 192.0.2.6 (192.0.2.6) 56(84) bytes of data.
64 bytes from 192.0.2.6: icmp_seq=1 ttl=61 time=2.55 ms
64 bytes from 192.0.2.6: icmp_seq=2 ttl=61 time=1.80 ms
64 bytes from 192.0.2.6: icmp_seq=3 ttl=61 time=1.94 ms

--- 192.0.2.6 ping statistics ---
3 packets transmitted, 3 received, 0% packet loss, time 2003ms
rtt min/avg/max/mdev = 1.804/2.097/2.552/0.325 ms
```
