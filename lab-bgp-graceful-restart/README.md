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

[1]: https://www.juniper.net/techpubs/en_US/junose10.3/information-products/topic-collections/swconfig-bgp-mpls/id-44002.html
