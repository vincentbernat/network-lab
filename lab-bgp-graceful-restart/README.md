This small lab is about debugging BGP graceful restart with BIRD.

# Problem 1

On R2:

    ip -ts monitor route

On R1:

    pkill bird
    bird -R -c /mnt/lab/bird.R1.conf

Despite graceful restart configured, we see that the route from R1 on
R2 is deleted and reinserted a few seconds later:

    [2016-12-02T10:10:21.266668] Deleted 203.0.113.0/24 via 192.0.2.1 dev eth0 proto bird
    [2016-12-02T10:10:27.836092] 203.0.113.0/24 via 192.0.2.1 dev eth0 proto bird

This is somewhat unexpected since graceful restart is enabled on all hosts.

## Investigation

On RR1, we see the session going to the down state:

    2016-12-02 10:09:24 <RMT> R1: Received: Administrative shutdown
    2016-12-02 10:09:24 <TRACE> R1: BGP session closed
    2016-12-02 10:09:24 <TRACE> R1: State changed to stop
    2016-12-02 10:09:24 <TRACE> R1 > removed [sole] 203.0.113.0/24 via 192.0.2.1 on eth0
    2016-12-02 10:09:24 <TRACE> R2 < removed 203.0.113.0/24 via 192.0.2.1 on eth0
    2016-12-02 10:09:25 <TRACE> R2: Sending UPDATE
    2016-12-02 10:09:25 <TRACE> R1: Down
    2016-12-02 10:09:25 <TRACE> R1: State changed to down

If graceful restart was correctly detected, we should see "Neighbor
graceful restart detected" from `bgp_handle_graceful_restart()`. This
function has two call sites: `bgp_sock_err()` and
`bgp_incoming_connection()`. We don't match the first one because we
would see a "Connection lost" or "Connection closed" message. We don't
match the second one either has we are closing a connection, not
establishing a new one was the previous one is stale.

It seems graceful restart only works for unexpected close. For
example, if we `pkill -9`, we now get:

    2016-12-02 10:52:50 <TRACE> R1: Neighbor graceful restart detected
    2016-12-02 10:52:50 <TRACE> R1: State changed to start
    2016-12-02 10:52:50 <TRACE> R1: BGP session closed
    2016-12-02 10:52:50 <TRACE> R1: Connect delayed by 5 seconds
    2016-12-02 10:52:51 <TRACE> R1: BFD session down
    2016-12-02 10:52:51 <TRACE> R1: State changed to stop
    2016-12-02 10:52:51 <TRACE> R1 > removed [sole] 203.0.113.0/24 via 192.0.2.1 on eth0
    2016-12-02 10:52:51 <TRACE> R2 < removed 203.0.113.0/24 via 192.0.2.1 on eth0
    2016-12-02 10:52:51 <TRACE> R1: Down
    
So, it's better, but now we have BFD forcing the session down and the
routes get removed again. If we disable BFD, everything works as
expected:

    2016-12-02 10:56:26 <TRACE> R1: Incoming connection from 192.0.2.1 (port 49303) accepted
    2016-12-02 10:56:26 <TRACE> R1: Sending OPEN(ver=4,as=65000,hold=240,id=01010101)
    2016-12-02 10:56:27 <TRACE> R1: Got OPEN(as=65000,hold=240,id=00000001)
    2016-12-02 10:56:27 <TRACE> R1: Sending KEEPALIVE
    2016-12-02 10:56:27 <TRACE> R1: Got KEEPALIVE
    2016-12-02 10:56:27 <TRACE> R1: BGP session established
    2016-12-02 10:56:27 <TRACE> R1: State changed to feed
    2016-12-02 10:56:27 <TRACE> R1 < added 198.51.100.0/24 via 192.0.2.2 on eth0
    2016-12-02 10:56:27 <TRACE> R1 < rejected by protocol 203.0.113.0/24 via 192.0.2.1 on eth0
    2016-12-02 10:56:27 <TRACE> R1: State changed to up
    2016-12-02 10:56:27 <TRACE> R1: Sending UPDATE
    2016-12-02 10:56:27 <TRACE> R1: Sending END-OF-RIB
    2016-12-02 10:56:30 <TRACE> R1: Got UPDATE
    2016-12-02 10:56:30 <TRACE> R1 > added [best] 203.0.113.0/24 via 192.0.2.1 on eth0
    2016-12-02 10:56:30 <TRACE> R2 < replaced 203.0.113.0/24 via 192.0.2.1 on eth0
    2016-12-02 10:56:30 <TRACE> R1 < rejected by protocol 203.0.113.0/24 via 192.0.2.1 on eth0
    2016-12-02 10:56:30 <TRACE> R2: Sending UPDATE
    2016-12-02 10:56:30 <TRACE> R1: Got UPDATE
    2016-12-02 10:56:30 <TRACE> R1: Got END-OF-RIB
    2016-12-02 10:56:30 <TRACE> R1: Neighbor graceful restart done

Therefore, graceful restart seems to be incompatible with
BFD. [Juniper][1] has special provisions to support BFD and graceful
restart.

> So that BFD can maintain its BFD protocol sessions across a BGP
> graceful restart, BGP requests that BFD set the C bit to 1 in
> transmitted BFD packets. When the C bit is set to 1, BFD can
> maintain its session in the forwarding plane in spite of disruptions
> in the control plane. Setting the bit to 1 gives BGP neighbors
> acting as a graceful restart helper the most accurate information
> about whether the forwarding plane is up.
>
> When BGP is acting as a graceful restart helper and the BFD session
> to the BGP peer is lost, one of the following actions takes place:
>  - If the C bit received in the BFD packets was 1, BGP immediately
>    flushes all routes, determining that the forwarding plane on the
>    BGP peer has gone down.
>  - If the C bit received in the BFD packets was 0, BGP marks all
>    routes as stale but does not flush them because the forwarding
>    plane on the BGP peer might be working and only the control plane
>    has gone down.

However, this means that BFD needs to continue to run. On JunOS,
*bgpd* and *bfdd* are two different processes. However, with BIRD,
this is not possible. When configuring a Juniper VRR with graceful
restart, we don't get graceful restart either due to this problem.

[1]: https://www.juniper.net/techpubs/en_US/junose10.3/information-products/topic-collections/swconfig-bgp-mpls/id-44002.html

# Problem 2

On R2:

    ip -ts monitor route

On R1:

    echo b > /proc/sysrq-trigger

On reboot, R1 is not able to immediately connect to RR1. RR1 says:

    2016-12-02 11:03:55 <TRACE> R1: Starting
    2016-12-02 11:03:55 <TRACE> R1: State changed to start
    2016-12-02 11:03:55 <TRACE> R1: Startup delayed by 60 seconds due to errors
    2016-12-02 11:04:02 <TRACE> R1: Incoming connection from 192.0.2.1 (port 49205) rejected
    2016-12-02 11:04:07 <TRACE> R1: Incoming connection from 192.0.2.1 (port 36449) rejected
    2016-12-02 11:04:10 <TRACE> R1: Incoming connection from 192.0.2.1 (port 59245) rejected
    2016-12-02 11:04:15 <TRACE> R1: Incoming connection from 192.0.2.1 (port 38099) rejected
    2016-12-02 11:04:22 <TRACE> R1: Incoming connection from 192.0.2.1 (port 37577) rejected
    2016-12-02 11:04:25 <TRACE> R1: Incoming connection from 192.0.2.1 (port 41783) rejected
    2016-12-02 11:04:29 <TRACE> R1: Incoming connection from 192.0.2.1 (port 58655) rejected
    2016-12-02 11:04:34 <TRACE> R1: Incoming connection from 192.0.2.1 (port 53057) rejected
    2016-12-02 11:04:39 <TRACE> R1: Incoming connection from 192.0.2.1 (port 57515) rejected
    2016-12-02 11:04:45 <TRACE> R1: Started
    2016-12-02 11:04:45 <TRACE> R1: Connect delayed by 5 seconds
    2016-12-02 11:04:45 <TRACE> R1: Incoming connection from 192.0.2.1 (port 47179) accepted
    2016-12-02 11:04:45 <TRACE> R1: Sending OPEN(ver=4,as=65000,hold=240,id=01010101)
    2016-12-02 11:04:46 <TRACE> R1: Got OPEN(as=65000,hold=240,id=00000001)
    2016-12-02 11:04:46 <TRACE> R1: Sending KEEPALIVE
    2016-12-02 11:04:46 <TRACE> R1: Got KEEPALIVE
    2016-12-02 11:04:46 <TRACE> R1: BGP session established

Moreover, the delay will be increased to 120 seconds the next time
(despite the successful connection between).

However, this is not related to graceful restart as the same behavior
can be triggered without graceful restart.

## Investigation

The message "Startup delayed by 60 seconds due to errors" also
triggers two actions:

    p->start_state = BSS_DELAY;
    bgp_start_timer(p->startup_timer, p->startup_delay);

When the incoming connection comes back, we have this test:

    acc = (p->p.proto_state == PS_START || p->p.proto_state == PS_UP) &&
      (p->start_state >= BSS_CONNECT) && (!p->incoming_conn.sk);

`BSS_DELAY` is less than `BSS_CONNECT`. Therefore, we get `acc = 0`.
Since we are not doing a graceful restart (since the previous
connection has been closed with BFD), the connection is rejected. For
incoming connection, maybe it would be easily fixable by accepting a
state of `BSS_DELAY`.

The delay is computed like this:

    if (!p->startup_delay)
      p->startup_delay = cf->error_delay_time_min;
    else
      p->startup_delay = MIN(2 * p->startup_delay, cf->error_delay_time_max);

Therefore, we can workaround the issue with `error wait time
1,30`. The fact that the error doubles even after a succesful
connection is due to amnesia. This can be configured with `error
forget time 30`.

The only "bug" left is the fact that an incoming connection cannot be
established due to a previous BFD error. Is that really a bug?
