# Implementation of EPF-uRPF

This is a tentative to implement
[draft-ietf-opsec-urpf-improvements][].

## Linux

When an AS is single-homed, no problem with uRPF in strict mode:

```console
C3# ping -c3 2001:db8:2::100
PING 2001:db8:2::100(2001:db8:2::100) 56 data bytes
64 bytes from 2001:db8:2::100: icmp_seq=1 ttl=62 time=0.598 ms
64 bytes from 2001:db8:2::100: icmp_seq=2 ttl=62 time=1.95 ms
64 bytes from 2001:db8:2::100: icmp_seq=3 ttl=62 time=1.77 ms

--- 2001:db8:2::100 ping statistics ---
3 packets transmitted, 3 received, 0% packet loss, time 2003ms
rtt min/avg/max/mdev = 0.598/1.440/1.953/0.600 ms
```

We cannot spoof:

```console
C3# ping -c3 -I 2001:db8:1d2::1 2001:db8:2::100
[...]
```

And on C2:

```console
C2# tcpdump -pni eth0
(nothing)
```

But with a multi-homed ASN, uRPF in strict mode doesn't work:

```console
C1# ping -c3 -I 2001:db8:1d3::1 2001:db8:2::100
PING 2001:db8:2::100(2001:db8:2::100) from 2001:db8:1d3::1 : 56 data bytes

--- 2001:db8:2::100 ping statistics ---
3 packets transmitted, 0 received, 100% packet loss, time 2030ms
```

We can switch to loose mode:

```console
AS64510# ip6tables -t raw -I RPFILTER -m rpfilter --loose -j RETURN
```

And ping starts working:

```console
C1# ping -c3 -I 2001:db8:1d3::1 2001:db8:2::100
PING 2001:db8:2::100(2001:db8:2::100) from 2001:db8:1d3::1 : 56 data bytes
64 bytes from 2001:db8:2::100: icmp_seq=1 ttl=61 time=0.863 ms
64 bytes from 2001:db8:2::100: icmp_seq=2 ttl=61 time=1.25 ms
64 bytes from 2001:db8:2::100: icmp_seq=3 ttl=61 time=1.79 ms

--- 2001:db8:2::100 ping statistics ---
3 packets transmitted, 3 received, 0% packet loss, time 2003ms
rtt min/avg/max/mdev = 0.863/1.300/1.789/0.379 ms
```

However, in loose mode, C3 can spoof source IP addresses:

```console
C3# ping -c3 -I 2001:db8:1d2::1 2001:db8:2::100
[...]
```

You can check `C2` receives the spoofed ICMP packets:

```console
C2# tcpdump -pni eth0
21:09:05.831417 IP6 2001:db8:1d2::1 > 2001:db8:2::100: ICMP6, echo request, seq 1, length 64
21:09:05.831439 IP6 2001:db8:2::100 > 2001:db8:1d2::1: ICMP6, echo reply, seq 1, length 64
21:09:06.860460 IP6 2001:db8:1d2::1 > 2001:db8:2::100: ICMP6, echo request, seq 2, length 64
21:09:06.860483 IP6 2001:db8:2::100 > 2001:db8:1d2::1: ICMP6, echo reply, seq 2, length 64
21:09:07.884679 IP6 2001:db8:1d2::1 > 2001:db8:2::100: ICMP6, echo request, seq 3, length 64
21:09:07.884709 IP6 2001:db8:2::100 > 2001:db8:1d2::1: ICMP6, echo reply, seq 3, length 64
```

[draft-ietf-opsec-urpf-improvements][] should solves this. However, it
seems difficult to implement it in Linux as is. Linux doesn't even
have the notion of feasible paths. If it was possible, the solution
would be, for each AS, to install fake routes on all peers the AS is
present (with a very high metric for example).

## Juniper

On Juniper, there is some flexibility on how uRPF is implemented as we
can use a policy. However, we would need to do a lookup on the AS
number of the incoming IP to check if it is known through the incoming
interface.

[draft-ietf-opsec-urpf-improvements]: https://tools.ietf.org/html/draft-ietf-opsec-urpf-improvements-04
