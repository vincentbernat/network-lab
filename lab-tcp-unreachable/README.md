# Effect of missing routes on established TCP session

When a route is missing, new TCP connections are immediately discarded:

```console
 17:37 CEST ❱ socat STDIO 'TCP-CONNECT:[2001:db8::2]:80'
2024/08/05 17:37:08 socat[704] E connect(5, AF=10 [2001:0db8:0000:0000:0000:0000:0000:0002]:80, 28): Network is unreachable
```

However, once established, a missing route does not reset the TCP session. To
test, on H1:

```console
 17:38 CEST ❱ socat STDIO 'TCP-CONNECT:[2001:db8::2]:80' &
[1] 713
 17:38 CEST ❱ ip route del 2001:db8::2 via 2001:db8:a::1
 17:38 CEST ❱ fg
[1]  + 713 continued  socat STDIO 'TCP-CONNECT:[2001:db8::2]:80'
ff
ff
```

When established:

```console
 17:40 CEST ❱ ss -taupen --extended --memory --info dport = 80
Netid     State     Recv-Q     Send-Q          Local Address:Port           Peer Address:Port    Process
tcp       ESTAB     0          0              [2001:db8:a::]:59822         [2001:db8::2]:80       users:(("socat",pid=745,fd=5))      ino:6036 sk:e cgroup:unreachable:1 <->
         skmem:(r0,rb131072,t0,tb87040,f0,w0,o0,bl0,d0) ts sack cubic wscale:5,5 rto:200 rtt:0.711/0.355 mss:1428 pmtu:1500 rcvmss:536 advmss:1428 cwnd:10 bytes_acked:1 segs_out:2 segs_in:1 send 160675105bps lastsnd:5308 lastrcv:5308 lastack:5308 pacing_rate 321350208bps delivered:1 app_limited rcv_space:14280 rcv_ssthresh:64108 minrtt:0.711 snd_wnd:64260 rcv_wnd:64800
```

When the route is missing but no packet was sent:

```console
 17:40 CEST ❱ ss -taupen --extended --memory --info dport = 80
Netid     State     Recv-Q     Send-Q          Local Address:Port           Peer Address:Port    Process
tcp       ESTAB     0          0              [2001:db8:a::]:59822         [2001:db8::2]:80       users:(("socat",pid=745,fd=5))      ino:6036 sk:e cgroup:unreachable:1 <->
         skmem:(r0,rb131072,t0,tb87040,f0,w0,o0,bl0,d0) ts sack cubic wscale:5,5 rto:200 rtt:0.711/0.355 mss:1428 pmtu:1500 rcvmss:536 advmss:1428 cwnd:10 bytes_acked:1 segs_out:2 segs_in:1 send 160675105bps lastsnd:27624 lastrcv:27624 lastack:27624 pacing_rate 321350208bps delivered:1 app_limited rcv_space:14280 rcv_ssthresh:64108 minrtt:0.711 snd_wnd:64260 rcv_wnd:64800
```

When some packets have been sent:

```console
 17:40 CEST ❱ ss -taupen --extended --memory --info dport = 80
Netid     State     Recv-Q     Send-Q          Local Address:Port           Peer Address:Port    Process
tcp       ESTAB     0          3              [2001:db8:a::]:59822         [2001:db8::2]:80       users:(("socat",pid=745,fd=5))      timer:(persist,768ms,3) ino:6036 sk:e cgroup:unreachable:1 <->
         skmem:(r0,rb131072,t0,tb87040,f3197,w899,o0,bl0,d0) ts sack cubic wscale:5,5 rto:200 backoff:3 rtt:0.711/0.355 mss:36 pmtu:68 rcvmss:536 advmss:1428 cwnd:10 bytes_sent:12 bytes_acked:1 segs_out:6 segs_in:1 data_segs_out:4 send 4050633bps lastsnd:832 lastrcv:32176 lastack:32176 pacing_rate 321350208bps delivered:1 app_limited busy:2300ms rcv_space:14280 rcv_ssthresh:64108 notsent:3 minrtt:0.711 snd_wnd:64260 rcv_wnd:64800
```

When sending packets, they are buffered in the send queue.
