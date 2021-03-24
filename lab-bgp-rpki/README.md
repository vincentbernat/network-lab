# Small lab with "private" RPKI validation

See [Securing BGP on the host with the
RPKI](https://vincent.bernat.ch/en/blog/2019-bgp-host-rpki) for more
details. You need BIRD 2.0.8.

We use RTR to validate announced prefixes. R1 is announcing a prefix
that should be accepted as well as an invalid one.

On R2, we accept the validated prefix but reject the other one:

    bird> show route
    Table master6:
    2001:db8:cccc::/48   unicast [bgp1 09:09:26.762] * (100) [AS65000i]
            via 2001:db8:aaaa::f on eth0
    
    Table r6:
    2001:db8:dddd::/48-48 AS65007  [rpki1 09:09:23.588] * (100)
    2001:db8:cccc::/48-48 AS65000  [rpki1 09:09:23.588] * (100)
    bird> show route filtered
    Table master6:
    2001:db8:dddd::/48   unicast [bgp1 09:09:26.762] * (100) [AS65000i]
            via 2001:db8:aaaa::f on eth0

On R3, we do the same:

    juniper@R3> show route protocol bgp all table inet6.0 extensive
    
    inet6.0: 9 destinations, 9 routes (8 active, 0 holddown, 1 hidden)
    2001:db8:cccc::/48 (1 entry, 0 announced)
            *BGP    Preference: 170/-101
                    Next hop type: Router, Next hop index: 0
                    Address: 0xd052410
                    Next-hop reference count: 2
                    Source: 2001:db8:aaaa::f
                    Next hop: 2001:db8:aaaa::f via em1.0, selected
                    Session Id: 0x0
                    State: <Active NotInstall Ext>
                    Local AS: 65002 Peer AS: 65000
                    Age: 1:02
                    Validation State: valid
                    Task: BGP_65000.2001:db8:aaaa::f+179
                    AS path: 65000 I
                    Accepted
                    Localpref: 100
                    Router ID: 1.1.1.1
    
    2001:db8:dddd::/48 (1 entry, 0 announced)
             BGP                 /-101
                    Next hop type: Router, Next hop index: 0
                    Address: 0xd052410
                    Next-hop reference count: 2
                    Source: 2001:db8:aaaa::f
                    Next hop: 2001:db8:aaaa::f via em1.0, selected
                    Session Id: 0x0
                    State: <Hidden NotInstall Ext>
                    Local AS: 65002 Peer AS: 65000
                    Age: 1:17
                    Validation State: unverified
                    Task: BGP_65000.2001:db8:aaaa::f+179
                    AS path: 65000 I
                    Localpref: 100
                    Router ID: 1.1.1.1
                    Hidden reason: rejected by import policy

Also, still on R3:

    juniper@R3> show validation session detail
    Session 2001:db8:bbbb::f, State: up, Session index: 2
      Group: RPKI, Preference: 100
      Port: 8282
      Refresh time: 300s
      Hold time: 600s
      Record Life time: 3600s
      Serial (Full Update): 1
      Serial (Incremental Update): 1
        Session flaps: 0
        Session uptime: 00:01:58
        Last PDU received: 00:01:58
        IPv4 prefix count: 0
        IPv6 prefix count: 2
    
    juniper@R3> show validation database brief
    RV database for instance master
    
    Prefix                 Origin-AS Session                                 State   Mismatch
    2001:db8:cccc::/48-48      65000 2001:db8:bbbb::f                        valid
    2001:db8:dddd::/48-48      65007 2001:db8:bbbb::f                        valid
    
      IPv4 records: 0
      IPv6 records: 2
