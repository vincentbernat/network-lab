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

# Problem 3

Even without BFD, graceful restart with a Juniper VRR doesn't work. As
soon as the Juniper VRR detects the BGP session is closed, the route
is expunged:

    Dec  5 13:46:40.056062 bgp_io_mgmt_cb: peer 192.0.2.1 (Internal AS 65000): USER_IND_ERROR event for I/O session Broken pipe - closing it
    Dec  5 13:46:40.056914 bgp_peer_close_and_restart: closing peer 192.0.2.1 (Internal AS 65000), state is 7 (Established) event Restart
    Dec  5 13:46:40.056924 bgp_send_deactivate:2368: 192.0.2.1 (Internal AS 65000) ,flags=0x8010002: removed from active list
    Dec  5 13:46:40.056935 bgp_event: peer 192.0.2.1 (Internal AS 65000) old state Established event Restart new state Idle
    Dec  5 13:46:40.057018 bgp_rt_unsync_all:1671: 192.0.2.1 (Internal AS 65000): entered v4nsync 2
    Dec  5 13:46:40.057029 bgp_oq_ready_enqueue:1340: group public-v4 type Internal: called for ribix 1, inserted node on thread
    Dec  5 13:46:40.057036 bgp_rt_nosync_bitreset: bgp (0xb7fa000) group public-v4 type Internal, bgp_nosync { 0xb7fa69c, 0xb7fa69c }
    Dec  5 13:46:40.057043 bgp_rt_unsync_all:1715: 192.0.2.1 (Internal AS 65000): end v4nsync 1
    Dec  5 13:46:40.057053 BGP peer 192.0.2.1 (Internal AS 65000) rt terminate: enqueue in close list, start close job (flags Unconfigured Closing GRHelperMark)
    Dec  5 13:46:40.058244 BGP peer 192.0.2.1 (Internal AS 65000) CLOSE: Threaded I/O session delete done event - do user cleanup
    Dec  5 13:46:40.058261 I/O session delete o-101-11-BGP_65000.192.0.2.1+57697 (0xad1f620): current bgp I/O session 0xad1f620, pending session count 0
    Dec  5 13:46:40.058270 BGP peer 192.0.2.1 (Internal AS 65000) CLOSE: Unlink theI/O session
    Dec  5 13:46:40.058276 I/O session delete o-101-11-BGP_65000.192.0.2.1+57697: primary session 0xad1f620 socket -1 unlinked, user cleanup completed
    Dec  5 13:46:40.058284 BGP peer 192.0.2.1 (Internal AS 65000) CLOSE: Check proceed with close? Rt terminate=In Progress, I/O session cleanup=Complete
    Dec  5 13:46:40.058322 BGP peer 192.0.2.1 (Internal AS 65000) CLOSE: I/O session delete completed synchronously, res 0, error 32
    Dec  5 13:46:40.058419 bgp_rti_terminate 192.0.2.1 (Internal AS 65000): peer close flags 0x9, rtt -, terminating=Y routes 0, bgp routes 1
    Dec  5 13:46:40.058495 CHANGE   203.0.113.0/24      nhid 0 gw zero-len        BGP      pref 170/-101 metric  <Delete Int Ext>  as 65000
    Dec  5 13:46:40.058505 bgp_rti_terminate 192.0.2.1 (Internal AS 65000): terminating, rt deleted
    Dec  5 13:46:40.058513 rt_close: 1/0 route proto BGP_RT_Background from
    Dec  5 13:46:40.058513
    Dec  5 13:46:40.058525 bgp_rti_terminate 192.0.2.1 (Internal AS 65000) complete, 96us saw 1 10416/s marked 0 deleted 1
    Dec  5 13:46:40.058545 bgp_advq_peer_clear_int: Clear bits on route 198.51.100.0 via BGP_Group_public-v4
    Dec  5 13:46:40.058556 bgp_reref_adv_helper_rt: 192.0.2.1 (Internal AS 65000) not re-referencing route 198.51.100.0 via peer_clear adv_entry: no afmets
    Dec  5 13:46:40.058566 bgp_bit_reset: 198.51.100.0 Clearing bit 0x20000
    Dec  5 13:46:40.058573 From 192.0.2.2
    Dec  5 13:46:40.058580 bgp_master_tsi_free: Freeing bgp_tsi_t for 198.51.100.0
    Dec  5 13:46:40.058591 bgp_advq_peer_clear_int: Clear bits on route 203.0.113.0via BGP_Group_public-v4
    Dec  5 13:46:40.058604 BGP close rib done: bcls 0xb809a80 (RIB Done)peer 192.0.2.1 (Internal AS 65000), group group public-v4 type Internal ribix 0x1
    Dec  5 13:46:40.058628 bgp_flush_grhelp_labels 192.0.2.1 (Internal AS 65000) nothing to do
    Dec  5 13:46:40.058634 BGP peer 192.0.2.1 (Internal AS 65000) CLOSE: Check proceed with close? Rt terminate=Complete, I/O session cleanup=Complete
    Dec  5 13:46:40.058647 bgp_peer_post_close: Cleanup complete for peer 192.0.2.1(last_flap Restart, state Idle, flags Unconfigured)
    Dec  5 13:46:40.058658 bgp_peer_post_close:6864 BGP peer 192.0.2.1 (Internal AS65000) CLOSE: post close cleanup - NO restart required, (last_flap Restart, state Idle, flags Unconfigured), group (flags , # peers 2)
    Dec  5 13:46:40.058672 BGP 192.0.2.1 (Internal AS 65000) unlink from group public-v4 type Internal
    Dec  5 13:46:40.058679 BGP peer 192.0.2.1 (Internal AS 65000) free
    Dec  5 13:46:40.058685 bgp_flush_grhelp_labels 192.0.2.1 (Internal AS 65000) nothing to do
    Dec  5 13:46:40.058738 bgp_flash_cb: rtt (null)(0xb924200) rib=1 flash type=1 cb target=group public-v4 type Internal
    Dec  5 13:46:40.058759 bgp_rto_count_update:2170: 192.0.2.2 (Internal AS 65000): rib 1, priority 0, updated count, 0 + 1 =  1
    Dec  5 13:46:40.058766 bgp_rt_timer_update_now: Forced update of route timer to1324 for group public-v4 type Internal
    Dec  5 13:46:40.058774 bgp_oq_ready_enqueue:1321: group public-v4 type Internal: called for ribix 1, node inserted into rtq_ready thread pri 0
    Dec  5 13:46:40.058779 bgp_oq_ready_enqueue:1328: group public-v4 type Internal: called for ribix 1, node already on rtq_ready thread
    Dec  5 13:46:40.058790 bgp_write_ready:3331: 192.0.2.2 (Internal AS 65000): Write ready, inserted in act list, write job started
    Dec  5 13:46:40.058799 bgp_rt_send_group_subr:3182: group public-v4 type Internal: table=1, state_grp=0xab27d80, Triggered write ready for 1 total and 1 sync peers, isflash=1, nextime -14872, start = 1324:177208, peers in sync 1
    Dec  5 13:46:40.058804 bgp_group_ready:7739: group public-v4 type Internal: ribix 0x1, isflash
    Dec  5 13:46:40.058812 Group ready, new=Y, grprib=0xb928a00 {sbits=0xc6fd4b0, sync=0xc6fd4b0, ribsync=Y, rtqdefer=N}, rtq ready=Y, act rib=1, nsync=1, rto nexttme=1324
    Dec  5 13:46:40.059922 brt_active_set:2722: send proc: active table set for peer 192.0.2.2 (Internal AS 65000), rib 1, priority 0
    Dec  5 13:46:40.059936 bgp_rt_send_active:3824: group=group public-v4 type Internal, priorities not done=1
    Dec  5 13:46:40.059945 bgp_rt_send_active_subr:3495: send proc: group=group public-v4 type Internal, stategrp=0xab27d80, act=0xadb8200 table 1 sending to peersfor priority 0
    Dec  5 13:46:40.060042 bgp_rt_send_active_subr:3566: group=group public-v4 typeInternal, table 1 sending to 1 peers for priority 0
    Dec  5 13:46:40.060058 bgp_rt_send_common:2076: group public-v4 type Internal: Start, 1324:178336
    Dec  5 13:46:40.060068 bgp_rt_send_common:2111: group public-v4 type Internal: rtop=0xa8fbe5c, prev (mrtop=0x0, mrtop_next=0x0, status=0x0 tokens=1)
    Dec  5 13:46:40.060078 Initialized group send msg bld area data=0xa8fbf4c
    Dec  5 13:46:40.060087 bgp_rt_send_attr_insert:1670: group public-v4 type Internal: Inserted attr into update, status=0x0
    Dec  5 13:46:40.060093 bgp_rt_send_update_check_init:1917: group public-v4 typeInternal: Initialized new update with attr, status=0x0
    Dec  5 13:46:40.060108 bgp_bit_reset: 203.0.113.0 Clearing bit 0x20000
    Dec  5 13:46:40.060116 bgp_master_tsi_free: Freeing bgp_tsi_t for 203.0.113.0
    Dec  5 13:46:40.060136 RELEASE  203.0.113.0/24      nhid 0 gw zero-len        BGP      pref 170/-101 metric  <Release Delete Int Ext>  as 65000
    Dec  5 13:46:40.060155 start @ mrtop=0x0
    Dec  5 13:46:40.060164 bgp_peer_send_msg_start:2325: Cloned new peer send msg into build area 0xc3b69b8 from group, parts=3, bytes=27
    Dec  5 13:46:40.060173 BGP_65000.192.0.2.2+47355: send proc: send via threaded I/O
    Dec  5 13:46:40.060183  wrote 27 bytes to I/O queue
    Dec  5 13:46:40.060189 finished number of messages 3, write qidx 1 rc 1
    Dec  5 13:46:40.060196 bgp_output_thrashold_reached: peer 192.0.2.2 (Internal AS 65000): rtt 0xb924200 id 0x1000000, change count 0, bgp threshold 5000
    Dec  5 13:46:40.060201 bgp_rt_send_message:1449: 192.0.2.2 (Internal AS 65000):send succeeded, written=27
    Dec  5 13:46:40.060208 bgp_rt_send_v4_flush:1754: 192.0.2.2 (Internal AS 65000): Flushed, len=0, status=0x0, updates 24, updates_bnp 24, tokens=0
    Dec  5 13:46:40.060214 bgp_send_flush:997: send proc: Flushed, type=1, status=0x0
    Dec  5 13:46:40.060220 bgp_group_send_msg_done:2099: group public-v4 type Internal: Reset/released group send msg bld area
    Dec  5 13:46:40.060226 bgp_send_handle_error:1324: group public-v4 type Internal: Flush type=GROUPP, status=0x0, num_tokens=0
    Dec  5 13:46:40.060237 bgp_send_handle_error:1422: group public-v4 type Internal: Flush type=GROUPP, status=0x0, Return status=0x20, num_tokens=0 - exit
    Dec  5 13:46:40.060242 rt send common: Exited loop normally - flushed partial -status=0x20
    Dec  5 13:46:40.060247 rt send common: END, status=0x20
    Dec  5 13:46:40.060252 bgp_group_send_msg_done:2099: group public-v4 type Internal: Reset/released group send msg bld area
    Dec  5 13:46:40.060260 bgp_rt_send_common_end:1496: 192.0.2.2 (Internal AS 65000): rib 1, priority 0, Decrement count 1 - 1 =  0
    Dec  5 13:46:40.060269 bgp_rt_send_common:2591: group public-v4 type Internal: Saved key BRT_KEY rib 1, pri 0, brt_key(0xffff7048), {gen=0, t=1324} mets=0x0, rt=0xac08274, addpath=N
    
    Dec  5 13:46:40.060278 bgp_rt_send_active_subr:3606: group public-v4 type Internal: sent: 1 timer expired(start = 1324:178336, nexttime = 1),  Flash deferred: 0 Peers blocked: 0 , status=0x20
    Dec  5 13:46:40.060285 brt_active_set:2722: send proc: active table set for peer 192.0.2.2 (Internal AS 65000), rib 1, priority 0
    Dec  5 13:46:40.060291 bgp_rt_send_active_subr:3659: group=group public-v4 typeInternal, Reinserted active markers, requeue_active=Y,- status=0x20
    Dec  5 13:46:40.060297 bgp_rt_send_active_subr:3671: group public-v4 type Internal: suspend/tokens, flush type = GROUPP, status = 0x20, ret status = 0x20, tokens = 0
    Dec  5 13:46:40.060303 bgp_rt_send_active:3872: group public-v4 type Internal: Sent priority 0 status 0x20 1 tokens used 0 tokens still left, last_used_nothingis 0
    Dec  5 13:46:40.060310 bgp_rt_send_active:3943: group public-v4 type Internal: refilling tokens
    Dec  5 13:46:40.060317 bgp_rt_send_active_subr:3495: send proc: group=group public-v4 type Internal, stategrp=0xab27d80, act=0xadb8200 table 1 sending to peersfor priority 0
    Dec  5 13:46:40.060323 bgp_rt_send_active_subr:3566: group=group public-v4 typeInternal, table 1 sending to 1 peers for priority 0
    Dec  5 13:46:40.060329 bgp_rt_send_common:2076: group public-v4 type Internal: Start, 1324:178336
    Dec  5 13:46:40.060347 rt send common: END, status=0x0
    Dec  5 13:46:40.060353 bgp_group_send_msg_done:2099: group public-v4 type Internal: Reset/released group send msg bld area
    Dec  5 13:46:40.060362 bgp_rt_send_common:2591: group public-v4 type Internal: Saved key BRT_KEY rib 1, pri 0, brt_key(0xffff7048), {gen=0, t=1324} mets=0x0, rt=0xac08274, addpath=N
    
    Dec  5 13:46:40.060371 bgp_rt_send_active_subr:3606: group public-v4 type Internal: sent: 0 timer expired(start = 1324:178336, nexttime = 1),  Flash deferred: 0 Peers blocked: 0 , status=0x0
    Dec  5 13:46:40.060378 bgp_rt_send_active_subr:3659: group=group public-v4 typeInternal, Reinserted active markers, requeue_active=N,- status=0x0
    Dec  5 13:46:40.060385 bgp_rt_send_active_subr:3688: group public-v4 type Internal: 1 peers processed a RIB, enqueue active queue of next RIB at marker rib_act=1, pri=0, status=0x0
    Dec  5 13:46:40.060392 bgp_rt_send_active:3872: group public-v4 type Internal: Sent priority 0 status 0x0 0 tokens used 1 tokens still left, last_used_nothing is 1
    Dec  5 13:46:40.060398 bgp_rt_send_active:3905: group public-v4 type Internal: priority=0, done
    Dec  5 13:46:40.060404 bgp_rt_send_active:3975: group public-v4 type Internal: END, status=0x0
    Dec  5 13:46:40.060410 bgp_write: group group public-v4 type Internal: wrote routes, status = 0x0
    Dec  5 13:46:40.060416 bgp_write: peer 192.0.2.2 (Internal AS 65000): wrote routes, status=SEND_OK, no more work
    Dec  5 13:46:40.060422 bgp_write:3263: 192.0.2.2 (Internal AS 65000): removed from active list
    Dec  5 13:46:40.060428 bgp_rt_sync:1471: 192.0.2.2 (Internal AS 65000): v4bits 1 v4nsync 1 peer insync y
    Dec  5 13:46:40.060434 bgp_rt_sync:1511: 192.0.2.2 (Internal AS 65000): remove from readyq
    Dec  5 13:46:40.060440 bgp_oq_ready_dequeue:1382: group public-v4 type Internal: delete for ribix 1
    Dec  5 13:46:41.059923 Group timer update, nexttime=0

## Diagnostic

Before "restart", we can check that the peers agree on the restart stuff:

    root@RR1# run show bgp neighbor 192.0.2.1
    Peer: 192.0.2.1+35445 AS 65000 Local: 192.0.2.254+179 AS 65000
      Group: public-v4             Routing-Instance: master
      Forwarding routing-instance: master
      Type: Internal    State: Established  (route reflector client)Flags: <Unconfigured Sync>
      Last State: OpenConfirm   Last Event: RecvKeepAlive
      Last Error: None
      Options: <Preference LocalAddress GracefulRestart Ttl Cluster Refresh>
      Local Address: 192.0.2.254 Holdtime: 90 Preference: 170
      Number of flaps: 0
      Peer ID: 0.0.0.1         Local ID: 1.1.1.1           Active Holdtime: 90
      Keepalive Interval: 30         Group index: 0    Peer index: 0
      I/O Session Thread: bgpio-0 State: Enabled
      BFD: disabled, down
      NLRI for restart configured on peer: inet-unicast
      NLRI advertised by peer: inet-unicast
      NLRI for this session: inet-unicast
      Peer supports Refresh capability (2)
      Restart time configured on the peer: 60
      Stale routes from peer are kept for: 60
      Restart time requested by this peer: 120
      Restart flag received from the peer: Restarting
      NLRI that peer supports restart for: inet-unicast
      NLRI peer can save forwarding state: inet-unicast
      NLRI that peer saved forwarding for: inet-unicast
      NLRI that restart is negotiated for: inet-unicast
      NLRI of received end-of-rib markers: inet-unicast
      NLRI of all end-of-rib markers sent: inet-unicast
      Peer does not support LLGR Restarter or Receiver functionality
      Peer supports 4 byte AS extension (peer-as 65000)
      Peer does not support Addpath
      Table inet.0 Bit: 20000
        RIB State: BGP restart is complete
        Send state: in sync
        Active prefixes:              1
        Received prefixes:            1
        Accepted prefixes:            1
        Suppressed due to damping:    0
        Advertised prefixes:          1
      Last traffic (seconds): Received 1968 Sent 4    Checked 1968
      Input messages:  Total 4      Updates 2       Refreshes 0     Octets 145
      Output messages: Total 0      Updates 1       Refreshes 0     Octets 104
      Output Queue[1]: 0            (inet.0, inet-unicast)
      Trace options: open, general, state, graceful-restart, bridge
      Trace file: /var/log/bgp size 131072 files 10

As per RFC 4724, it is said:

> When the Receiving Speaker detects termination of the TCP session for
> a BGP session with a peer that has advertised the Graceful Restart
> Capability, it MUST retain the routes received from the peer for all
> the address families that were previously received in the Graceful
> Restart Capability and MUST mark them as stale routing information.

So, there is something wrong here.

If we declare neighbors explicitely with `neighbor` instead of using
`allow`, this works:

    Dec  5 14:34:01.372167 bgp_io_mgmt_cb: peer 192.0.2.1 (Internal AS 65000): USER_IND_ERROR event for I/O session Broken pipe - closing it
    Dec  5 14:34:01.373425 bgp_peer_close_and_restart: closing peer 192.0.2.1 (Internal AS 65000), state is 7 (Established) event Restart
    Dec  5 14:34:01.373437 bgp_send_deactivate:2368: 192.0.2.1 (Internal AS 65000) ,flags=0x8010000: removed from active list
    Dec  5 14:34:01.373452 bgp_event: peer 192.0.2.1 (Internal AS 65000) old state Established event Restart new state Idle
    Dec  5 14:34:01.373520 bgp_rt_unsync_all:1671: 192.0.2.1 (Internal AS 65000): entered v4nsync 2
    Dec  5 14:34:01.373530 bgp_oq_ready_enqueue:1340: group public-v4 type Internal: called for ribix 1, inserted node on thread
    Dec  5 14:34:01.373537 bgp_rt_nosync_bitreset: bgp (0xb7fa000) group public-v4 type Internal, bgp_nosync { 0xb7fa69c, 0xb7fa69c }
    Dec  5 14:34:01.373543 bgp_rt_unsync_all:1715: 192.0.2.1 (Internal AS 65000): end v4nsync 1
    Dec  5 14:34:01.373553 BGP peer 192.0.2.1 (Internal AS 65000) rt terminate: enqueue in close list, start close job (flags Closing GRHelperMark)
    Dec  5 14:34:01.374745 BGP peer 192.0.2.1 (Internal AS 65000) CLOSE: Threaded I/O session delete done event - do user cleanup
    Dec  5 14:34:01.374755 I/O session delete o-98-3-BGP_65000.192.0.2.1 (0xad1f620): current bgp I/O session 0xad1f620, pending session count 0
    Dec  5 14:34:01.374762 BGP peer 192.0.2.1 (Internal AS 65000) CLOSE: Unlink theI/O session
    Dec  5 14:34:01.374766 I/O session delete o-98-3-BGP_65000.192.0.2.1: primary session 0xad1f620 socket -1 unlinked, user cleanup completed
    Dec  5 14:34:01.374774 BGP peer 192.0.2.1 (Internal AS 65000) CLOSE: Check proceed with close? Rt terminate=In Progress, I/O session cleanup=Complete
    Dec  5 14:34:01.374789 BGP peer 192.0.2.1 (Internal AS 65000) CLOSE: I/O session delete completed synchronously, res 0, error 32

Above messages are the same. First difference is:

    Dec  5 14:34:01.374905 bgp_rti_terminate 192.0.2.1 (Internal AS 65000): peer close flags 0x1, rtt -, terminating=N routes 0, bgp routes 1
    Dec  5 14:34:01.374923 bgp_mark_route_stale 192.0.2.1 (Internal AS 65000): peerflags 0x134218752, peer's rst flags 0x8000, rt 203.0.113.0/24
    Dec  5 14:34:01.374934 bgp_rti_terminate 192.0.2.1 (Internal AS 65000): rt 203.0.113.0/24, marked stale
    Dec  5 14:34:01.374942 bgp_rti_terminate 192.0.2.1 (Internal AS 65000) complete, 25us saw 1 40000/s marked 1 deleted 0
    Dec  5 14:34:01.374962 bgp_advq_peer_clear_int: Clear bits on route 198.51.100.0 via BGP_Group_public-v4
    Dec  5 14:34:01.375007 bgp_reref_adv_helper_rt: 192.0.2.1 (Internal AS 65000) not re-referencing route 198.51.100.0 via peer_clear adv_entry: no afmets
    Dec  5 14:34:01.375020 bgp_bit_reset: 198.51.100.0 Clearing bit 0x20000
    Dec  5 14:34:01.375027 From 192.0.2.2
    Dec  5 14:34:01.375034 bgp_master_tsi_free: Freeing bgp_tsi_t for 198.51.100.0
    Dec  5 14:34:01.375048 bgp_advq_peer_clear_int: Clear bits on route 203.0.113.0via BGP_Group_public-v4
    Dec  5 14:34:01.375058 BGP close rib done: bcls 0xba83a80 (RIB Done)peer 192.0.2.1 (Internal AS 65000), group group public-v4 type Internal ribix 0x1
    Dec  5 14:34:01.375067 BGP peer 192.0.2.1 (Internal AS 65000) CLOSE: Check proceed with close? Rt terminate=Complete, I/O session cleanup=Complete
    Dec  5 14:34:01.375077 bgp_peer_post_close: Cleanup complete for peer 192.0.2.1(last_flap Restart, state Idle, flags )
    Dec  5 14:34:01.375087 bgp_peer_post_close:6917 BGP Peer 192.0.2.1 (Internal AS65000) CLOSE: post close cleanup - NEED restart, (last_flap Restart, state Idle, flags )
    Dec  5 14:34:01.375094 bgp_event: peer 192.0.2.1 (Internal AS 65000) old state Idle event Start new state Active
