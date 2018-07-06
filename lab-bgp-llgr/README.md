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

This has been tested with:

 - vRR 16.1R2.11 (no problem)
 - vRR 17.3R1.10 (ECMP routes broken but otherwise work)

## Commands

The lab has 3 nodes:

 - a Juniper vRR split into two logical systems,
 - a Linux node running GoBGP with LLGR enabled (it needs a patch for interop)
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

On vRR 16.1R2.11, it doesn't seem possible to use an import policy to
match a stale route. The displayed community may have been added after
import policies are evaluated.

We can check we can ping the destination despite the ongoing "DDoS"
(better than nothing):

    juniper@R:R1> ping 2001:db8:10::2 count 10 rapid
    PING6(56=40+8+8 bytes) 2001:db8:2::1 --> 2001:db8:10::2
    !!.!.!.!.!
    --- 2001:db8:10::2 ping6 statistics ---
    10 packets transmitted, 6 packets received, 40% packet loss
    round-trip min/avg/max/std-dev = 0.580/1.516/2.251/0.677 ms

## Documentation

Long-lived BGP graceful restart is still a draft. It is implemented by
Juniper and Cisco.

 - [Support for Long-lived BGP Graceful Restart](https://datatracker.ietf.org/doc/draft-uttaro-idr-bgp-persistence/)
 - [Juniper documentation](https://www.juniper.net/documentation/en_US/junos/topics/concept/bgp-long-lived-graceful-restart.html)

## Interoperability

I didn't try to get the value used for LLGR stale community in JunOS.
This is only a minor compatibility problem if they differ between
implementations.

### GoBGP

GoBGP supports BGP LLGR since quite some time but can interoperate
with JunOS since 1.33. The regular graceful restart timer cannot be
set to 0 as this would disable everything. Therefore, it needs to be
set to 1. GoBGP seems to correctly advertise BGP capabilities as a
LLGR speaker and restarter but it doesn't mark routes as stale once
the BGP session is over:

     $ gobgp neighbor 2001:db8:102::1 adj-in
     Neighbor 2001:db8:102::1's BGP session is not established

This needs to be investigated.

### BIRD

From JunOS point of view:

    Peer: 2001:db8:104::4+60605 AS 65000 Local: 2001:db8:104::1+179 AS 65000
      Group: peers                 Routing-Instance: master
      Forwarding routing-instance: master
      Type: Internal    State: Established    Flags: <Sync>
      Last State: OpenConfirm   Last Event: RecvKeepAlive
      Last Error: None
      Export: [ LOOPBACK NOTHING ]
      Options: <Preference HoldTime Ttl AddressFamily Multipath Refresh>
      Options: <BfdEnabled LLGR>
      Address families configured: inet6-unicast
      Holdtime: 6 Preference: 170
      NLRI inet6-unicast:
      Number of flaps: 1
      Last flap event: Restart
      Peer ID: 1.0.0.4         Local ID: 1.0.0.1           Active Holdtime: 6
      Keepalive Interval: 2          Group index: 0    Peer index: 4
      I/O Session Thread: bgpio-0 State: Enabled
      BFD: enabled, up
      NLRI for restart configured on peer: inet6-unicast
      NLRI advertised by peer: inet6-unicast
      NLRI for this session: inet6-unicast
      Peer supports Refresh capability (2)
      Stale routes from peer are kept for: 300
      Restart time requested by this peer: 0
      NLRI that peer supports restart for: inet6-unicast
      NLRI peer can save forwarding state: inet6-unicast
      NLRI that restart is negotiated for: inet6-unicast
      NLRI of received end-of-rib markers: inet6-unicast
      NLRI of all end-of-rib markers sent: inet6-unicast
      NLRI and times for LLGR configured for peer: inet6-unicast 00:02:00
      NLRI and times that peer supports LLGR Restarter for: inet6-unicast 00:02:00
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
      Last traffic (seconds): Received 3150 Sent 2496 Checked 3150
      Input messages:  Total 1251   Updates 2       Refreshes 0     Octets 23899
      Output messages: Total 1364   Updates 1       Refreshes 0     Octets 26060
      Output Queue[1]: 0            (inet6.0, inet6-unicast)

We are missing the following line:

    NLRI that peer saved LLGR forwarding for: inet6-unicast

From BIRD point of view:

    BIRD 1.6.4 ready.
    name     proto    table    state  since       info
    R1_1     BGP      master   up     10:35:03    Established
      Preference:     100
      Input filter:   ACCEPT
      Output filter:  ACCEPT
      Routes:         1 imported, 1 exported, 1 preferred
      Route change stats:     received   rejected   filtered    ignored   accepted
        Import updates:              2          0          0          0          3
        Import withdraws:            0          0        ---          0          0
        Export updates:             12         10          0        ---          2
        Export withdraws:            1        ---        ---        ---          0
      BGP state:          Established
        Neighbor address: 2001:db8:104::1
        Neighbor AS:      65000
        Neighbor ID:      1.0.0.1
        Neighbor caps:    refresh restart-able llgr-able AS4
        Session:          internal AS4
        Source address:   2001:db8:104::4
        Hold timer:       5/6
        Keepalive timer:  2/2

From JunOS point of view once LLGR kicks in:

    juniper@R:R1> show bgp neighbor 2001:db8:104::4
    Peer: 2001:db8:104::4+179 AS 65000 Local: 2001:db8:104::1+57667 AS 65000
      Group: peers                 Routing-Instance: master
      Forwarding routing-instance: master
      Type: Internal    State: Connect        Flags: <>
      Last State: Active        Last Event: ConnectRetry
      Last Error: None
      Export: [ LOOPBACK NOTHING ]
      Options: <Preference HoldTime Ttl AddressFamily Multipath Refresh>
      Options: <BfdEnabled LLGR>
      Address families configured: inet6-unicast
      Holdtime: 6 Preference: 170
      NLRI inet6-unicast:
      Number of flaps: 2
      Last flap event: Restart
      Time until long-lived stale routes deleted: inet6-unicast 00:01:05
      Table inet6.0 Bit: 20000
        RIB State: BGP restart is complete
        Send state: not advertising
        Active prefixes:              0
        Received prefixes:            1
        Accepted prefixes:            1
        Suppressed due to damping:    0
        LLGR-stale prefixes:          1

Also:

         BGP    Preference: 170/-101
                Next hop type: Indirect, Next hop index: 0
                Address: 0xc776e10
                Next-hop reference count: 1
                Source: 2001:db8:104::4
                Next hop type: Router, Next hop index: 778
                Next hop: 2001:db8:104::4 via em1.104, selected
                Session Id: 0x14a
                Protocol next hop: 2001:db8:104::4
                Indirect next hop: 0xb1d27c0 1048578 INH Session ID: 0x15c
                State: <Int Ext>
                Inactive reason: LLGR stale
                Local AS: 65000 Peer AS: 65000
                Age: 4  Metric2: 0
                Validation State: unverified
                Task: BGP_65000.2001:db8:104::4
                AS path: I
                Communities: llgr-stale
                Accepted LongLivedStale
                Localpref: 100
                Router ID: 1.0.0.4
                Indirect next hops: 1
                        Protocol next hop: 2001:db8:104::4
                        Indirect next hop: 0xb1d27c0 1048578 INH Session ID: 0x15c
                        Indirect path forwarding next hops: 1
                                Next hop type: Router
                                Next hop: 2001:db8:104::4 via em1.104
                                Session Id: 0x14a
                        2001:db8:104::/120 Originating RIB: inet6.0
                          Node path count: 1
                          Forwarding nexthops: 1
                                Next hop type: Interface
                                Nexthop: via em1.104

From BIRD point of view:

    name     proto    table    state  since       info
    R1_1     BGP      master   start  11:20:17    Connect
      Preference:     100
      Input filter:   ACCEPT
      Output filter:  ACCEPT
      Routes:         1 imported, 0 exported, 0 preferred
      Route change stats:     received   rejected   filtered    ignored   accepted
        Import updates:              2          0          0          0          4
        Import withdraws:            0          0        ---          0          0
        Export updates:             12         10          0        ---          2
        Export withdraws:            1        ---        ---        ---          0
      BGP state:          Connect
        Neighbor address: 2001:db8:104::1
        Neighbor AS:      65000
        Neighbor graceful restart active
        LL stale timer:   32/-

The BGP connection is correctly torn down by BFD (so, no risk of
sending an hold timer expired message). Also:

    2001:db8:10::1/128 via 2001:db8:204::1 on eth0.204 [R1_2 10:35:01] * (100) [i]
            Type: BGP unicast univ
            BGP.origin: IGP
            BGP.as_path:
            BGP.next_hop: 2001:db8:204::1 fe80::5254:3300:cc00:5
            BGP.local_pref: 100
                       via 2001:db8:104::1 on eth0.104 [R1_1 11:22:51] (100s) [i]
            Type: BGP unicast univ
            BGP.origin: IGP
            BGP.as_path:
            BGP.next_hop: 2001:db8:104::1 fe80::5254:3300:6800:5
            BGP.local_pref: 100
            BGP.community: (65535,6)

The route gets the community and the "stale" bit. The stale route is
not used to build the ECMP route, except if we only have stale routes.

    2001:db8:10::1 via 2001:db8:204::1 dev eth0.204 proto bird metric 1024 pref medium
