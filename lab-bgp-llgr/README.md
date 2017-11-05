# Lab for BGP Long-Lived Graceful Restart

This is a mechanism to keep routes even when the BGP session goes
down, but at a lower priority. Notably, this enables routing to keep
working in case of disruption in the control plane while not keeping
dead routes after a real failure.

Consider the following requisites:

 - we want routes to be withdrawed immediately in case of a
   communication failure (link down on the path)
 - we want routes to be kept when control plane is disrupted

Both requirements seem to contradict but we can reconcile them by
keeping withdrawed routes only as last resort routes. This is exactly
what BGP Long-Lived Graceful Restart is doing.

It provides two mechanisms:

 - an extension to graceful restart to mark and handle long-lived
   stale routes (configurable timer) with a lower priority to normal
   routes,

 - a community to use to send those routes to other BGP routers
   understanding the extension.

We use only the first mechanism in this lab. Also, graceful restart is
not enabled explicitely. This means the long-lived stale route timer
starts immediately.

## Commands

The lab has two nodes:

 - a Juniper vRR split into two logical systems,
 - a Linux node acting as a switch and able to simulate some kind of
   control plane failure by dropping BFD and BGP packets.

LLGR is completely configured by the `llgr` group. Therefore, it's
easy to test what happens without and without. We focus on `R1` (use
`set cli logical-system R1`).

When everything is correct, three BGP sessions are established:

    juniper@R:R1> show bgp summary
    Groups: 3 Peers: 3 Down peers: 0
    Table          Tot Paths  Act Paths Suppressed    History Damp State    Pending
    inet6.0
                           3          3          0          0          0          0
    Peer                     AS      InPkt     OutPkt    OutQ   Flaps Last Up/Dwn State|#Active/Received/Accepted/Damped...
    2001:db8:1::2         65000          3          2       0       0          19 Establ
      inet6.0: 1/1/1/0
    2001:db8:2::2         65000          4          0       0       1          19 Establ
      inet6.0: 1/1/1/0
    2001:db8:3::2         65000          4          0       0       1          13 Establ
      inet6.0: 1/1/1/0

The same route is learnt over all peers:

    juniper@R:R1> show route protocol bgp
    
    inet6.0: 12 destinations, 14 routes (12 active, 0 holddown, 0 hidden)
    + = Active Route, - = Last Active, * = Both
    
    2001:db8:10::2/128 *[BGP/170] 00:00:34, localpref 100, from 2001:db8:1::2
                          AS path: I, validation-state: unverified
                          to 2001:db8:1::2 via em1.0
                        > to 2001:db8:2::2 via em2.0
                          to 2001:db8:3::2 via em3.0
                        [BGP/170] 00:00:39, localpref 100
                          AS path: I, validation-state: unverified
                        > to 2001:db8:2::2 via em2.0
                        [BGP/170] 00:00:34, localpref 100
                          AS path: I, validation-state: unverified
                        > to 2001:db8:3::2 via em3.0

LLGR is enabled on each neighbor:

    juniper@R:R1> show bgp neighbor 2001:db8:1::2
    Peer: 2001:db8:1::2+179 AS 65000 Local: 2001:db8:1::1+62976 AS 65000
      Group: peer1                 Routing-Instance: master
      Forwarding routing-instance: master
      Type: Internal    State: Established    Flags: <Sync>
      Last State: OpenConfirm   Last Event: RecvKeepAlive
      Last Error: Open Message Error
      Export: [ LOOPBACK NOTHING ]
      Options: <Preference Ttl AddressFamily Multipath Refresh>
      Options: <BfdEnabled LLGR>
      Address families configured: inet6-unicast
      Holdtime: 90 Preference: 170
      NLRI inet6-unicast:
      Number of flaps: 0
      Error: 'Open Message Error' Sent: 1 Recv: 0
      Peer ID: 1.0.0.2         Local ID: 1.0.0.1           Active Holdtime: 90
      Keepalive Interval: 30         Group index: 0    Peer index: 0
      I/O Session Thread: bgpio-0 State: Enabled
      BFD: enabled, up
      NLRI for restart configured on peer: inet6-unicast
      NLRI advertised by peer: inet6-unicast
      NLRI for this session: inet6-unicast
      Peer supports Refresh capability (2)
      Stale routes from peer are kept for: 300
      Restart time requested by this peer: 0
      Restart flag received from the peer: Notification
      NLRI that peer supports restart for: inet6-unicast
      NLRI peer can save forwarding state: inet6-unicast
      NLRI that peer saved forwarding for: inet6-unicast
      NLRI that restart is negotiated for: inet6-unicast
      NLRI of received end-of-rib markers: inet6-unicast
      NLRI of all end-of-rib markers sent: inet6-unicast
      NLRI and times for LLGR configured for peer: inet6-unicast 00:02:00
      NLRI and times that peer supports LLGR Restarter for: inet6-unicast 00:02:00
      NLRI that peer saved LLGR forwarding for: inet6-unicast
      Peer supports 4 byte AS extension (peer-as 65000)
      Peer does not support Addpath
      Table inet6.0 Bit: 20000
        RIB State: BGP restart is complete
        Send state: in sync
        Active prefixes:              1
        Received prefixes:            1
        Accepted prefixes:            1
        Suppressed due to damping:    0
        Advertised prefixes:          1
      Last traffic (seconds): Received 419  Sent 191  Checked 419
      Input messages:  Total 10     Updates 2       Refreshes 0     Octets 277
      Output messages: Total 9      Updates 1       Refreshes 0     Octets 277
      Output Queue[1]: 0            (inet6.0, inet6-unicast)

Notably:

    juniper@R:R1> show bgp neighbor 2001:db8:1::2 | match llgr
      Options: <BfdEnabled LLGR>
      NLRI and times for LLGR configured for peer: inet6-unicast 00:02:00
      NLRI and times that peer supports LLGR Restarter for: inet6-unicast 00:02:00
      NLRI that peer saved LLGR forwarding for: inet6-unicast

If we simulate a control-plane problem with only one neighbor (using
`ddos wire1` on the Linux box), the route is updated accordingly:

    juniper@R:R1> show bgp summary
    Groups: 3 Peers: 3 Down peers: 1
    Table          Tot Paths  Act Paths Suppressed    History Damp State    Pending
    inet6.0
                           3          2          0          0          0          0
    Peer                     AS      InPkt     OutPkt    OutQ   Flaps Last Up/Dwn State|#Active/Received/Accepted/Damped...
    2001:db8:1::2         65000          8          5       0       2           2 Active
      inet6.0: 0/1/1/0
    2001:db8:2::2         65000          9          5       0       2        2:25 Establ
      inet6.0: 1/1/1/0
    2001:db8:3::2         65000          9          8       0       1        2:30 Establ
      inet6.0: 1/1/1/0
    
    juniper@R:R1> show route protocol bgp
    
    inet6.0: 12 destinations, 14 routes (12 active, 0 holddown, 0 hidden)
    + = Active Route, - = Last Active, * = Both
    
    2001:db8:10::2/128 *[BGP/170] 00:00:09, localpref 100
                          AS path: I, validation-state: unverified
                        > to 2001:db8:2::2 via em2.0
                          to 2001:db8:3::2 via em3.0
                        [BGP/170] 00:02:37, localpref 100
                          AS path: I, validation-state: unverified
                        > to 2001:db8:3::2 via em3.0
                        [BGP/170] 00:00:09, localpref 100
                          AS path: I, validation-state: unverified
                        > to 2001:db8:1::2 via em1.0

Notably, the details of the last route say:

    juniper@R:R1> show route protocol bgp extensive inactive-path
    
    inet6.0: 12 destinations, 14 routes (12 active, 0 holddown, 0 hidden)
    2001:db8:10::2/128 (3 entries, 1 announced)
    TSI:
    KRT in-kernel 2001:db8:10::2/128 -> {indirect(1048574)}
             BGP    Preference: 170/-101
                    Next hop type: Indirect, Next hop index: 0
                    Address: 0xc7887d0
                    Next-hop reference count: 1
                    Source: 2001:db8:1::2
                    Next hop type: Router, Next hop index: 722
                    Next hop: 2001:db8:1::2 via em1.0, selected
                    Session Id: 0x148
                    Protocol next hop: 2001:db8:1::2
                    Indirect next hop: 0xb1d2400 1048576 INH Session ID: 0x149
                    State: <Int Ext>
                    Inactive reason: LLGR stale
                    Local AS: 65000 Peer AS: 65000
                    Age: 48         Metric2: 0
                    Validation State: unverified
                    Task: BGP_65000.2001:db8:1::2
                    AS path: I
                    Communities: llgr-stale
                    Accepted LongLivedStale
                    Localpref: 100
                    Router ID: 1.0.0.2
                    Indirect next hops: 1
                            Protocol next hop: 2001:db8:1::2
                            Indirect next hop: 0xb1d2400 1048576 INH Session ID: 0x149
                            Indirect path forwarding next hops: 1
                                    Next hop type: Router
                                    Next hop: 2001:db8:1::2 via em1.0
                                    Session Id: 0x148
                            2001:db8:1::/120 Originating RIB: inet6.0
                              Node path count: 1
                              Forwarding nexthops: 1
                                    Next hop type: Interface
                                    Nexthop: via em1.0

Notably:

    juniper@R:R1> show route protocol bgp extensive inactive-path | match "llgr|long"
                    Inactive reason: LLGR stale
                    Communities: llgr-stale
                    Accepted LongLivedStale

If we simulate a failure on the second link, we get:

    juniper@R:R1> show bgp summary
    Groups: 3 Peers: 3 Down peers: 2
    Table          Tot Paths  Act Paths Suppressed    History Damp State    Pending
    inet6.0
                           3          1          0          0          0          0
    Peer                     AS      InPkt     OutPkt    OutQ   Flaps Last Up/Dwn State|#Active/Received/Accepted/Damped...
    2001:db8:1::2         65000          0          0       0       3        1:08 Connect
      inet6.0: 0/1/1/0
    2001:db8:2::2         65000         17         12       0       3           1 Active
      inet6.0: 0/1/1/0
    2001:db8:3::2         65000         18         16       0       1        6:21 Establ
      inet6.0: 1/1/1/0
    
    juniper@R:R1> show route protocol bgp
    
    inet6.0: 12 destinations, 14 routes (12 active, 0 holddown, 0 hidden)
    + = Active Route, - = Last Active, * = Both
    
    2001:db8:10::2/128 *[BGP/170] 00:06:28, localpref 100
                          AS path: I, validation-state: unverified
                        > to 2001:db8:3::2 via em3.0
                        [BGP/170] 00:01:15, localpref 100
                          AS path: I, validation-state: unverified
                        > to 2001:db8:1::2 via em1.0
                        [BGP/170] 00:00:08, localpref 100
                          AS path: I, validation-state: unverified
                        > to 2001:db8:2::2 via em2.0

But once we make the third link fails, the route doesn't disappear:

    juniper@R:R1> show route protocol bgp
    
    inet6.0: 12 destinations, 14 routes (12 active, 0 holddown, 0 hidden)
    + = Active Route, - = Last Active, * = Both
    
    2001:db8:10::2/128 *[BGP/170] 00:00:05, localpref 100, from 2001:db8:1::2
                          AS path: I, validation-state: unverified
                          to 2001:db8:1::2 via em1.0
                        > to 2001:db8:2::2 via em2.0
                          to 2001:db8:3::2 via em3.0
                        [BGP/170] 00:00:39, localpref 100
                          AS path: I, validation-state: unverified
                        > to 2001:db8:2::2 via em2.0
                        [BGP/170] 00:00:05, localpref 100
                          AS path: I, validation-state: unverified
                        > to 2001:db8:3::2 via em3.0

The prefixes stay as long as the timer is not expired (two minutes in
our example):

    juniper@R:R1> show bgp neighbor 2001:db8:3::2 | match "llgr|long"
      Options: <BfdEnabled LLGR>
      Time until long-lived stale routes deleted: inet6-unicast 00:00:47
        LLGR-stale prefixes:          1

## Documentation

Long-lived BGP graceful restart is still a draft. It is implemented by
Juniper and Cisco.

 - [Support for Long-lived BGP Graceful Restart](https://datatracker.ietf.org/doc/draft-uttaro-idr-bgp-persistence/)
 - [Juniper documentation](https://www.juniper.net/documentation/en_US/junos/topics/concept/bgp-long-lived-graceful-restart.html)
