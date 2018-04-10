# Multihomed VXLAN BGP EVPN with Juniper vQFX

This lab needs a pretty recent FRR. With some patches:

 - [bgpd: add an option for RT auto-derivation to use RFC 8635](https://github.com/FRRouting/frr/pull/2034)
 - [bgpd: add basic support for ETI and ESI for BGP EVPN](https://github.com/FRRouting/frr/pull/2035)
 - [bgpd: don't add router's mac if all zero](https://github.com/FRRouting/frr/pull/2041)

This still doesn't work with multi-homed segments (hosted by remote VTEPs).

## DF election

    juniper@QFX1> show evpn instance default-switch extensive
    Instance: default-switch
      Route Distinguisher: 192.0.2.11:1
      Encapsulation type: VXLAN
      Duplicate MAC detection threshold: 5
      Duplicate MAC detection window: 180
      MAC database status                     Local  Remote
        MAC advertisements:                       2       1
        MAC+IP advertisements:                    0       0
        Default gateway MAC advertisements:       0       0
      Number of local interfaces: 1 (1 up)
        Interface name  ESI                            Mode             Status     AC-Role
        ae1.0           00:01:01:01:01:01:01:01:01:01  all-active       Up         Root
      Number of IRB interfaces: 0 (0 up)
      Number of protect interfaces: 0
      Number of bridge domains: 3
        VLAN  Domain ID   Intfs / up    IRB intf   Mode             MAC sync  IM route label  SG sync  IM core nexthop
        654   654            1    1                Extended         Enabled   654             Disabled
        655   655            1    1                Extended         Enabled   655             Disabled
        656   656            1    1                Extended         Enabled   656             Disabled
      Number of neighbors: 2
        Address               MAC    MAC+IP        AD        IM        ES Leaf-label
        192.0.2.12              0         0         0         3         0
        192.0.2.13              1         0         0         2         0
      Number of ethernet segments: 1
        ESI: 00:01:01:01:01:01:01:01:01:01
          Status: Resolved by IFL ae1.0
          Local interface: ae1.0, Status: Up/Forwarding
          Number of remote PEs connected: 1
            Remote PE        MAC label  Aliasing label  Mode
            192.0.2.12       655        0               single-homed
          DF Election Algorithm: MOD based
          Designated forwarder: 192.0.2.11
          Backup forwarder: 192.0.2.12
          Last designated forwarder update: Apr 10 11:33:47
      Router-ID: 192.0.2.11

## ESI advertisements

    juniper@QFX1> show route advertising-protocol bgp 192.0.2.100 extensive
    
    default-switch.evpn.0: 15 destinations, 15 routes (15 active, 0 holddown, 0 hidden)
    * 1:192.0.2.11:1::010101010101010101::0/192 AD/EVI (1 entry, 1 announced)
     BGP group evpn type Internal
         Route Distinguisher: 192.0.2.11:1
         Nexthop: Self
         Localpref: 100
         AS path: [65000] I
         Communities: target:65000:1 encapsulation:vxlan(0x8)
    
    * 2:192.0.2.11:1::654::50:54:33:00:00:15/304 MAC/IP (1 entry, 1 announced)
     BGP group evpn type Internal
         Route Distinguisher: 192.0.2.11:1
         Route Label: 654
         ESI: 00:01:01:01:01:01:01:01:01:01
         Nexthop: Self
         Localpref: 100
         AS path: [65000] I
         Communities: target:65000:268436110 encapsulation:vxlan(0x8)
    
    * 3:192.0.2.11:1::654::192.0.2.11/248 IM (1 entry, 1 announced)
     BGP group evpn type Internal
         Route Distinguisher: 192.0.2.11:1
         Route Label: 654
         PMSI: Flags 0x0: Label 654: Type INGRESS-REPLICATION 192.0.2.11
         Nexthop: Self
         Localpref: 100
         AS path: [65000] I
         Communities: target:65000:268436110 encapsulation:vxlan(0x8)
         PMSI: Flags 0x0: Label 40: Type INGRESS-REPLICATION 192.0.2.11
    
    * 3:192.0.2.11:1::655::192.0.2.11/248 IM (1 entry, 1 announced)
     BGP group evpn type Internal
         Route Distinguisher: 192.0.2.11:1
         Route Label: 655
         PMSI: Flags 0x0: Label 655: Type INGRESS-REPLICATION 192.0.2.11
         Nexthop: Self
         Localpref: 100
         AS path: [65000] I
         Communities: target:65000:268436111 encapsulation:vxlan(0x8)
         PMSI: Flags 0x0: Label 40: Type INGRESS-REPLICATION 192.0.2.11
    
    * 3:192.0.2.11:1::656::192.0.2.11/248 IM (1 entry, 1 announced)
     BGP group evpn type Internal
         Route Distinguisher: 192.0.2.11:1
         Route Label: 656
         PMSI: Flags 0x0: Label 656: Type INGRESS-REPLICATION 192.0.2.11
         Nexthop: Self
         Localpref: 100
         AS path: [65000] I
         Communities: target:65000:268436112 encapsulation:vxlan(0x8)
         PMSI: Flags 0x0: Label 41: Type INGRESS-REPLICATION 192.0.2.11
    
    __default_evpn__.evpn.0: 3 destinations, 3 routes (3 active, 0 holddown, 0 hidden)
    
    * 1:192.0.2.11:0::010101010101010101::FFFF:FFFF/192 AD/ESI (1 entry, 1 announced)
     BGP group evpn type Internal
         Route Distinguisher: 192.0.2.11:0
         Route Label: 1
         Nexthop: Self
         Localpref: 100
         AS path: [65000] I
         Communities: target:65000:1 encapsulation:vxlan(0x8) esi-label:0x0:all-active (label 0)
    
    * 4:192.0.2.11:0::010101010101010101:192.0.2.11/296 ES (1 entry, 1 announced)
     BGP group evpn type Internal
         Route Distinguisher: 192.0.2.11:0
         Route Label: 1
         Nexthop: Self
         Localpref: 100
         AS path: [65000] I
         Communities: encapsulation:vxlan(0x8) es-import-target:1-1-1-1-1-1
