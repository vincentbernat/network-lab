# Asymmetric VXLAN routing with BGP EVPN

This works if the target VXLAN has a local IRB. Then, traffic will be
VXLAN encapsulated (using the remote VNI). While the lab doesn't work
on vQFX for some reason, it works on real hardware (a QFX5110 with
18.1R3-S2.5).

The ARP reply is received correctly on `irb.584`:

    13:40:04.477461  In
            Juniper PCAP Flags [Ext, In], PCAP Extension(s) total length 22
              Device Media Type Extension TLV #3, length 1, value: Ethernet (1)
              Logical Interface Encapsulation Extension TLV #6, length 1, value: Ethernet (14)
              Device Interface Index Extension TLV #1, length 2, value: 641
              Logical Interface Index Extension TLV #4, length 4, value: 551
              Logical Unit Number Extension TLV #5, length 4, value: 584
            -----original packet-----
            50:54:33:00:00:0e > 02:05:86:71:45:00, ethertype 802.1Q (0x8100), length 60: vlan 584, p 0, ethertype ARP, arp reply 172.27.2.10 is-at 50:54:33:00:00:0e

QFX2 knows how to forward that:

    juniper@QFX2# ...e destination 02:05:86:71:45:00 extensive bridge-domain vlan584
    Routing table: default-switch.evpn-vxlan [Index 7]
    Bridging domain: vlan584.evpn-vxlan [Index 3]
    VPLS:
    Enabled protocols: Bridging, ACKed by all peers, EVPN VXLAN,
    
    Destination:  02:05:86:71:45:00/48
      Learn VLAN: 0                        Route type: user
      Route reference: 0                   Route interface-index: 572
      Multicast RPF nh index: 0
      P2mpidx: 0
      IFL generation: 131                  Epoch: 0
      Sequence Number: 0                   Learn Mask: 0x4000000000000000000000000000000000000000
      L2 Flags: control_dyn
      Flags: sent to PFE
      Nexthop:
      Next-hop type: composite             Index: 1745     Reference: 7
      Next-hop type: indirect              Index: 131070   Reference: 3
      Nexthop: 172.29.1.1
      Next-hop type: unicast               Index: 1744     Reference: 4
      Next-hop interface: xe-0/0/1.0

The PFE also knows:

    FXPC0(QFX2 vty)# show nhdb id 1745 extensive
       ID      Type      Interface    Next Hop Addr    Protocol       Encap     MTU               Flags  PFE internal Flags
    -----  --------  -------------  ---------------  ----------  ------------  ----  ------------------  ------------------
     1745    Compst  -              -                    BRIDGE             -     0  0x0000000000000000  0x0000000000000000
    
    BFD Session Id: 0
    
    Composite NH:
      Function: Vxlan Encap NH with forwarding NH being Unicast
      Hardware Index: 0x0
      Composite flag: 0x0
      Composite pfe flag: 0xe
      Lower-level NH Ids: 131070
      Derived NH Ids:
    
    Vxlan data:
            SIP = 0xac1d0102
            DIP = 0xac1d0101
            L3RTT = 0
            SVTEP ifl = 555
            proto = 2
            RVTEP ifl = 572
         VPFE: 0 PFE : 0 Port: 0 Flags: 0x0
         Egress_NH: 131070 egr_dsc_install: 1 egr_dsc_install_mask: 0x1
         Agg_member: 0 num_tags: 0 tokens_per_app: 2 n_cnhs = 0
        L2 Rewrite[0]: 00
        MPLS TAGS: num_of_tags = 0
    
    Application type: "DEFAULT"
    ===================================================
    
    for fe_id = 0, Ingress NH handle:
    handle 0x207f28c0, flags 0x0, refcount 1
    NH installed at addr NH 894, INT_SEQ 0x10001bf1
    Raw dump of the nh words of size 1 words
            0x100001b2
    SEQ [100001b2]  Interm, SIZE 1, NO_ACT 0, USE_REMAP 0, NEXT_ADDR: 00036, SZ: 2
    Nexthop points to:
    SEQ [10001be9]  Interm, SIZE 2, NO_ACT 0, USE_REMAP 0, NEXT_ADDR: 0037d, SZ: 1
    Nexthop points to:
    SEQ [10001a09]  Interm, SIZE 1, NO_ACT 0, USE_REMAP 0, NEXT_ADDR: 00341, SZ: 1
    Nexthop points to:
    SEQ [20280001]   Final, SIZE 1, EGPRT_VAL 1, PRT_TYP 1, VPFE 000, GRPID 01,
    
    
    
    
    Egress Next-hop on pfe inst 0:
    ------------------------------------
    
    L2 descriptor
    ==============
    
    des-type    p_next    Shared?   Primary     Last        desc_count  app-type    des-addr
                                    des-size    des-size
    --------    -----     -------   --------    --------    --------    --------    --------
    Private     No        Yes(1    )0           4           1           0           0x31520
    
    Des addrs       : 0x31520
    
    
        T: P_TUNNEL_ENCAP  tun_ptr: 0, incL2Len = 20, incL3Len = 20
    
    Tunnel tbl idx:    0 (Refcount:     1)
    
    Hdr_type = TUN_HDR_TYPE_IPV4_UDP
    Buff_seq = TUNNEL_BUFF_SEQ_TMPLT_MPLS_TUN
    Write control Flags:
        TUN_WR_CNTL_ERW_UDP_CONFIG_REGISTER[0]
    Tunnel Encap buffer contents (num_of_bytes = 23):
     buff[00] = 0xb5  buff[01] = 0x8e  buff[02] = 0x00  buff[03] = 0x45  buff[04] = 0x00
     buff[05] = 0x00  buff[06] = 0x00  buff[07] = 0x00  buff[08] = 0x00  buff[09] = 0x00
     buff[10] = 0x00  buff[11] = 0x40  buff[12] = 0x11  buff[13] = 0xcd  buff[14] = 0xce
     buff[15] = 0xac  buff[16] = 0x1d  buff[17] = 0x01  buff[18] = 0x02  buff[19] = 0x00
     buff[20] = 0x00  buff[21] = 0x00  buff[22] = 0x00
    
        T:TUNNEL_IPV4_DIP  tun_ipv4_dest: ac1d0101
        F:         P_NEXT  p_next: 201921
    
    Flabel descr for App "DEFAULT" Flabel_id: 323701:
    ============================================================
    
    des-type    p_next    Shared?   Primary     Last        desc_count  app-type    des-addr
                                    des-size    des-size
    --------    -----     -------   --------    --------    --------    --------    --------
    Public      No        No (0    )3           2           1           0           0x1495
    
    Des addrs       : 0x1495
    
     Flabel : 323701 Segment table  index: : 79[1] Page Table index : 144[9] desc start addr:: 5269[1]
    
        F:        COUNTER  counter: 128971  cix_tc_en: 0
        F:         P_NEXT  p_next: 202016
    
    Egress Next-hop on pfe inst 0:
    ------------------------------------
    
    L2 descriptor
    ==============
    
    des-type    p_next    Shared?   Primary     Last        desc_count  app-type    des-addr
                                    des-size    des-size
    --------    -----     -------   --------    --------    --------    --------    --------
    Private     No        Yes(1    )0           4           1           0           0x31524
    
    Des addrs       : 0x31524
    
    
        F:            TIX  tix: 4
        T: P_TUNNEL_ENCAP  tun_ptr: 57, incL2Len = 20, incL3Len = 20
    
    Tunnel tbl idx:   57 (Refcount:     1)
    
    Hdr_type = TUN_HDR_TYPE_IPV4_UDP
    Buff_seq = TUNNEL_BUFF_SEQ_TUN_TMPLT_MPLS
    Write control Flags:
        TUN_WR_CNTL_ERW_UDP_CONFIG_REGISTER[0]
    Tunnel Encap buffer contents (num_of_bytes = 23):
     buff[00] = 0x85  buff[01] = 0x8e  buff[02] = 0x00  buff[03] = 0x45  buff[04] = 0x00
     buff[05] = 0x00  buff[06] = 0x00  buff[07] = 0x00  buff[08] = 0x00  buff[09] = 0x00
     buff[10] = 0x00  buff[11] = 0x40  buff[12] = 0x11  buff[13] = 0xcd  buff[14] = 0xce
     buff[15] = 0xac  buff[16] = 0x1d  buff[17] = 0x01  buff[18] = 0x02  buff[19] = 0x00
     buff[20] = 0x00  buff[21] = 0x00  buff[22] = 0x00
    
        T:TUNNEL_IPV4_DIP  tun_ipv4_dest: ac1d0101
        F:         P_NEXT  p_next: 201922
    
    Flabel descr for App "DEFAULT" Flabel_id: 323698:
    ============================================================
    
    des-type    p_next    Shared?   Primary     Last        desc_count  app-type    des-addr
                                    des-size    des-size
    --------    -----     -------   --------    --------    --------    --------    --------
    Public      No        No (0    )3           2           1           0           0x1492
    
    Des addrs       : 0x1492
    
     Flabel : 323698 Segment table  index: : 79[1] Page Table index : 144[9] desc start addr:: 5266[1]
    
        F:        COUNTER  counter: 128971  cix_tc_en: 0
        F:         P_NEXT  p_next: 202020
    
    ctr_idx 0x1f7cb,                2 pkts,              124 bytes
    
      Routing-table id: 7
