# vMX advertising various route types using BMP

```
juniper@vMX> show route protocol bgp hidden

inet.0: 13 destinations, 16 routes (11 active, 0 holddown, 5 hidden)
+ = Active Route, - = Last Active, * = Both

192.0.2.0/31        [BGP ] 00:00:24, localpref 100
                      AS path: 65011 I, validation-state: unverified
                    >  to 192.0.2.1 via lt-0/0/0.0
192.0.2.4/31        [BGP ] 00:00:24, localpref 100
                      AS path: I, validation-state: unverified
                    >  to 192.0.2.5 via lt-0/0/0.4
192.0.2.6/31        [BGP ] 00:00:24, localpref 100, from 2001:db8::7
                      AS path: 65017 I, validation-state: unverified
                    >  to 192.0.2.7 via lt-0/0/0.6
198.51.100.0/25     [BGP ] 00:00:24, localpref 100
                      AS path: 65011 65011 174 1299 64476 ?, validation-state: unverified
                    >  to 192.0.2.1 via lt-0/0/0.0
198.51.100.128/25   [BGP ] 00:00:24, localpref 100
                      AS path: 65011 65011 174 29447 396919 E, validation-state: unverified
                    >  to 192.0.2.1 via lt-0/0/0.0

bgp.l3vpn.0: 5 destinations, 5 routes (0 active, 0 holddown, 5 hidden)
+ = Active Route, - = Last Active, * = Both

65017:101:198.51.100.0/25
                    [BGP ] 00:00:23, from 2001:db8::7
                      AS path: 65017 65017 174 1299 64476 ?, validation-state: unverified
                    >  to 192.0.2.7 via lt-0/0/0.6
65017:101:198.51.100.128/25
                    [BGP ] 00:00:23, from 2001:db8::7
                      AS path: 65017 65017 174 29447 396919 E, validation-state: unverified
                    >  to 192.0.2.7 via lt-0/0/0.6
65017:102:198.51.100.0/25
                    [BGP ] 00:00:23, from 2001:db8::7
                      AS path: 65017 65017 174 3356 3356 3356 64476 ?, validation-state: unverified
                    >  to 192.0.2.7 via lt-0/0/0.6
65017:102:198.51.100.128/25
                    [BGP ] 00:00:23, from 2001:db8::7
                      AS path: 65017 65017 6453 396919 E, validation-state: unverified
                    >  to 192.0.2.7 via lt-0/0/0.6
65017:103:198.51.100.0/26
                    [BGP ] 00:00:23, from 2001:db8::7
                      AS path: 65017 65017 3356 64476 ?, validation-state: unverified
                    >  to 192.0.2.7 via lt-0/0/0.6

inet6.0: 9 destinations, 13 routes (7 active, 0 holddown, 6 hidden)
+ = Active Route, - = Last Active, * = Both

2001:db8::2/127     [BGP ] 00:00:24, localpref 100
                      AS path: 65013 I, validation-state: unverified
                    >  to 2001:db8::3 via lt-0/0/0.2
2001:db8::6/127     [BGP ] 00:00:24, localpref 100
                      AS path: 65017 I, validation-state: unverified
                    >  to 2001:db8::7 via lt-0/0/0.6
2001:db8:1::/64     [BGP ] 00:00:24, localpref 100
                      AS path: 65017 65013 174 174 174 ?, validation-state: unverified
                    >  to 2001:db8::7 via lt-0/0/0.6
                    [BGP ] 00:00:24, localpref 100
                      AS path: 65013 65013 174 174 174 ?, validation-state: unverified
                    >  to 2001:db8::3 via lt-0/0/0.2
2001:db8:2::/64     [BGP ] 00:00:24, localpref 100
                      AS path: 65017 65017 1299 1299 1299 12322 E, validation-state: unverified
                    >  to 2001:db8::7 via lt-0/0/0.6
                    [BGP ] 00:00:24, localpref 100
                      AS path: 65013 65013 1299 1299 1299 12322 E, validation-state: unverified
                    >  to 2001:db8::3 via lt-0/0/0.2

bgp.l3vpn-inet6.0: 1 destinations, 1 routes (0 active, 0 holddown, 1 hidden)
+ = Active Route, - = Last Active, * = Both

65017:101:2001:db8:4::/64
                    [BGP ] 00:00:23
                      AS path: 65017 65017 1299 1299 1299 29447 I, validation-state: unverified
                    >  to 2001:db8::7 via lt-0/0/0.6

bgp.evpn.0: 1 destinations, 1 routes (0 active, 0 holddown, 1 hidden)
+ = Active Route, - = Last Active, * = Both

5:65017:103::0::198.51.100.0::26/248
                    [BGP ] 00:00:24, localpref 100
                      AS path: 65017 65017 3356 64476 ?, validation-state: unverified
                    >  to 2001:db8::7 via lt-0/0/0.6

juniper@vMX> show bgp bmp
Station name: collector
  Local address/port: -/-, Station address/port: 203.0.113.1/10179, active
  State: established Local: 203.0.113.0+54280 Remote: 203.0.113.1+10179
  Last state change: 24:08
  Monitor BGP Peers: enabled
  Route-monitoring: pre-policy
  Hold-down: 30, flaps 10, period 30
  Priority: low
  Statistics timeout: 3600
  Version: 3
  Routing Instance: default
```

All routes can be withdrawn by applying the `withdraw-all` group.
