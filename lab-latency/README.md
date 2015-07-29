# Lab for testing high latency links

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

## ss

On the other side, the utility `ss` will give plenty of information (depending on the kernel version):

    $ ss -tn --info --extended --memory --process --options src 203.0.113.2
    State      Recv-Q Send-Q               Local Address:Port                 Peer Address:Port
    ESTAB      0      1116408                 203.0.113.2:80                      192.0.2.2:49062  timer:(on,208ms,0) users:(("nginx",pid=213,fd=10)) uid:33 ino:8291 sk:ffff88001df43040 <->
             skmem:(r0,rb374400,t0,tb3394560,f44288,w3355392,o0,bl0) ts cubic wscale:6,6 rto:304 rtt:101.248/1.899 ato:40 mss:1448 cwnd:193 ssthresh:193 send 22.1Mbps pacing_rate 176.4Mbps unacked:771 retrans:1/512 lost:1 sacked:597 reordering:15 rcv_space:28960

 - `Recv-Q` and `Send-Q` depends on the socket state. For a socket in
   the `LISTEN` state, `Recv-Q` is the current size of the listen
   queue (number of connections the socket has accepted on the behalf
   of the application) while `Send-Q` is the initial size of the
   listen queue (as set by the `listen()` syscall). For other sockets,
   `Recv-Q` is the amount of bytes sitting in the kernel waiting for
   the application to read, while `Send-Q` is the amount of bytes
   sitting in the kernel waiting to be acknowledged by the remote
   party.

 - The timer shows what kind of timer is currently running for the
   given TCP socket and when it will trigger. The possible timers are
   `off` (no timer running), `on` (retransmit when this timer will
   expire), `keepalive` (a TCP keepalive will be sent), `timewait`
   (timer to expire the `TIMEWAIT` state), `persist` (a zero window
   probe will be sent because the receiver has reduced its window to
   zero, see RFC 1122 and RFC 6429). The additional number is the
   number of retransmissions/probes sent while the timer stay of the
   same kind.

 - The users is a list of processes having an open file descriptor to
   the socket. You get the name, the PID and the file descriptor
   number for this process.

 - `uid` is the UID of the user who created the socket.

 - `ino` is the inode affected to the socket. It acts as an unique identifier.

 - `sk` is the location of the socket in memory.

 - `skmem` describes the socket memory usage. All are fields of the
   `struct sock` structure:

      - `r` is `sk_rmem_alloc`,
      - `rb` is `sk_rcvbuf` (size of receive buffer in bytes),
      - `t` is `sk_wmem_alloc` (transmit queue bytes committed),
      - `tb` is `sk_sndbuf` (size of send buffer in bytes),
      - `f` is `sk_forward_alloc` (space allocated forward),
      - `w` is `sk_wmem_queued` (persistent queue size),
      - `o` is `sk_omem_alloc` (optional memory, used for example for filters),
      - `bl` is `sk_backlog.len` (memory for backlog).

   0, it means, that no bytes are actually waiting in the kernel for
   this socket. This amount also accounts for extra data needed to
   handle the socket. `rb` is the maximum memory the socket may use
   for reception (`sk_rcvbuf`). `t` and `tb` are for the transmit part
   (`sk_wmem_alloc` and `. `f` is the amount of free bytes the socket
   can has. `w` is the amount of bytes that should be sent (either not
   sent or not yet acknowledged). `o` is the amount of bytes for
   "optional memory". Filters attached to a socket uses optional
   memory. `bl` is the backlog size.

 - `ts` means timestamps are enabled.

 - `cubic` is the congestion algorithm used on this socket.

 - `wscale` is the window scale factor (congestion window, receiving window).

 - `rto` is retransmission timeout in milliseconds. It sets an upper
    value before acknowledgments come back. This should be compared
    with RTT. See the
    [following article](http://sgros.blogspot.fr/2012/02/calculating-tcp-rto.html).

 - `rtt` is the actual RTT has determined using TCP timestamps and
    the variance. The variance is used in computing RTO.

 - `cwnd` is the congestion window. The actual value should be
   obtained by multiplying this value with the MSS.

 - `send` is the theoritical bandwidth computed from `cwnd`, `mss` and
   `rtt` (bandwidth-delay product).

 - `ssthresh` is the TCP slow start threshold. Because of the use of
    fast recovery, this is the value used to fallback the congestion
    window in case the system has to send several duplicate
    acks. Above this threshold, the congestion window should increase
    of MSS/CWND.

 - `rcv_space` is twice (??) the advertised receive window in bytes.

 - `lastsnd`, `lastrcv` and `lastack` are the time in milliseconds
   since now something got sent, received or acked on the current
   socket. When the value is 0, it is not displayed.

 - `pacing_rate` is the rate which has been computed to avoid sending
   bursts of packets when the other side won't be able to process all
   of them. Currently, it is only used when using the
   [fair queuing scheduler](https://lwn.net/Articles/565421/) or to
   decide how much segments can be stuffed into a TSO packet. If there
   is a second number, it means that the application has forced a
   maximum pacing rate (with `SO_MAX_PACING_RATE`). The pacing rate is
   computed in `tcp_update_pacing_rate()`.

 - `unacked`, `retrans`, `lost`, `sacked` and `reordering` are
   statistics for this socket. All those numbers should be low.

## Benchmark tool

There is also a benchmark tool (written in Python 3): `benchmark`. It
uses `iperf3`. Run it like this:

     /lab/benchmark run --remote-stats NewYork

The results are in a CSV file. You can graph them with:

     /lab/benchmark graph results.csv

## Additional documentation

 - http://www.psc.edu/index.php/networking/641-tcp-tune
 - http://www.netcraftsmen.com/tcpip-performance-factors/
 - http://www.netcraftsmen.com/tcp-performance-and-the-mathis-equation/
