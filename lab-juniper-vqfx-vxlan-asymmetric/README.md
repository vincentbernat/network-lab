# Asymmetric VXLAN routing with BGP EVPN

This is a tentative. This doesn't seem to work as expected as traffic
is routed between QFX1 and QFX2 instead of being encapsulated.

    juniper@QFX1> show evpn database
    Instance: default-switch
    VLAN  DomainId  MAC address        Active source                  Timestamp        IP address
         583        00:00:5e:00:01:01  05:00:00:fd:e8:00:00:02:47:00  Mar 01 10:04:34  172.27.1.1
         583        02:05:86:71:8e:00  irb.583                        Mar 01 10:04:34  172.27.1.2
         583        50:54:33:00:00:0d  00:01:04:00:00:00:00:00:00:18  Mar 01 10:04:54  172.27.1.10
         584        00:00:5e:00:01:01  05:00:00:fd:e8:00:00:02:48:00  Mar 01 10:04:33  172.27.2.1
         584        02:05:86:71:f9:00  172.29.1.2                     Mar 01 10:04:33  172.27.2.2
         584        50:54:33:00:00:0e  00:01:04:00:00:00:00:00:00:18  Mar 01 10:10:21  172.27.2.10

Despite the information about 172.27.2.10 being on another VLAN, no
encapsulation happens. Let's dig more...
