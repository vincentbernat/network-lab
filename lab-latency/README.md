Lab for testing high latency links
==================================

Two hosts are connected through "Internet" using an emulated high
latency links: a router will add 50ms latency and some packet loss
when transmitting packets (with netem).

The goal of this lab is to tune TCP to use the maximum available
bandwidth despite the high latency.

The speed can be observed with `curl`:

    $ for i in $(seq 1 5); do
    >    curl --silent -w '%{http_code}   %{time_total}s \t%{size_download} \t%{speed_download}\n' \
    >      -4 http://NewYork/100M -o /dev/null
    > done
    200   33,120s   104857601       3165961,000
    200   29,028s   104857601       3612339,000
    200   25,087s   104857601       4179794,000
    200   34,724s   104857601       3019773,000
    200   38,561s   104857601       2719272,000

We can observe the influence of the IP layer (with IPv6):

    $ for i in $(seq 1 5); do
    >    curl --silent -w '%{http_code}   %{time_total}s \t%{size_download} \t%{speed_download}\n' \
    >      -6 http://NewYork/100M -o /dev/null
    > done
    200   38,130s   104857601       2750016,000
    200   28,462s   104857601       3684084,000
    200   26,459s   104857601       3962959,000
    200   30,471s   104857601       3441244,000
    200   30,801s   104857601       3404351,000

Or the influence of SACK (disabled here):

    $ sysctl -qw net.ipv4.tcp_sack=0 net.ipv4.tcp_dsack=0
    $ for i in $(seq 1 5); do
    >    curl --silent -w '%{http_code}   %{time_total}s \t%{size_download} \t%{speed_download}\n' \
    >      -4 http://NewYork/100M -o /dev/null
    > done
    200   117,343s  104857601       893601,000
    200   95,665s   104857601       1096093,000
    200   105,195s  104857601       996792,000
    200   133,774s  104857601       783838,000
    200   73,612s   104857601       1424465,000

On the other side, the utility `ss` will give plenty of information (depending on the kernel version):

    $ ss -tn --info --extended --memory --process --options src 203.0.113.2
    State      Recv-Q Send-Q               Local Address:Port                 Peer Address:Port
    ESTAB      0      1116408                 203.0.113.2:80                      192.0.2.2:49062  timer:(on,208ms,0) users:(("nginx",pid=213,fd=10)) uid:33 ino:8291 sk:ffff88001df43040 <->
             skmem:(r0,rb374400,t0,tb3394560,f44288,w3355392,o0,bl0) ts cubic wscale:6,6 rto:304 rtt:101.248/1.899 ato:40 mss:1448 cwnd:193 ssthresh:193 send 22.1Mbps pacing_rate 176.4Mbps unacked:771 retrans:1/512 lost:1 sacked:597 reordering:15 rcv_space:28960
