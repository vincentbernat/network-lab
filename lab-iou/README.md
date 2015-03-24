Lab with Cisco IOU
==================

Cisco IOU (or Cisco IOL) is a version of IOS running on Linux. The
advantage over using things like Dynamips/Dynagen is the better CPU
usage. You can find more information about it on
[Jeremy Gaddis' FAQ](http://evilrouters.net/2011/01/18/cisco-iou-faq/).

Lab
---

This lab is quite simple. Two IOU instances and two Linux running BIRD
are plugged on the same virtual switch and establish OSPF adjacencies
between them (with BFD for faster convergence times).

    IOU1>show ip ospf neighbor
    
    Neighbor ID     Pri   State           Dead Time   Address         Interface
    1.1.1.1           1   FULL/BDR        00:00:39    192.0.2.1       Ethernet0/0
    2.2.2.2           1   FULL/DR         00:00:39    192.0.2.2       Ethernet0/0
    4.4.4.4           1   2WAY/DROTHER    00:00:34    192.0.2.4       Ethernet0/0

    IOU1>show ip ospf rib
    
                OSPF Router with ID (3.3.3.3) (Process ID 1)
    
    
                    Base Topology (MTID 0)
    
    OSPF local RIB
    Codes: * - Best, > - Installed in global RIB
    
    *   192.0.2.0/24, Intra, cost 10, area 0, Connected
          via 192.0.2.3, Ethernet0/0
    *>  198.51.100.101/32, Intra, cost 10, area 0
          via 192.0.2.1, Ethernet0/0
    *>  198.51.100.102/32, Intra, cost 10, area 0
          via 192.0.2.2, Ethernet0/0
    *   198.51.100.103/32, Intra, cost 1, area 0, Connected
          via 198.51.100.103, Loopback0
    *>  198.51.100.104/32, Intra, cost 11, area 0
          via 192.0.2.4, Ethernet0/0

    IOU1>show bfd neighbors
    
    IPv4 Sessions
    NeighAddr                              LD/RD         RH/RS     State     Int
    192.0.2.1                               1/632309085  Up        Up        Et0/0
    192.0.2.2                               2/2718792012 Up        Up        Et0/0

The transmit interval for BFD is 200 ms but it can be reduced on real
hardware. It seems that on Cisco, only fully adjacent routers are
using BFD.

Images
------

There is some duplicity when people talks about IOU. Only a few
persons at Cisco are authorized to use it. However, if you search for
`Cisco IOU Collection`, you should find something suitable on
Bittorrent.

Once you have obtained your IOU images, link your favorite pick to
`images/i86bi-linux-l3.bin`. Please note that older images may be
missing a lot of features (for example, VLAN, port channels). More
recent ones (15.x) are more likely to have the features you need. See
[Andrea Dainese's blog post](http://www.routereflector.com/cisco/cisco-iou-web-interface/features-not-supported/)
for some details on this.

You may need to patch it if you get an error like this:

    $ ../images/i86bi-linux-l3.bin --help
    ../images/i86bi-linux-l3.bin: error while loading shared libraries: libcrypto.so.4: cannot open shared object file: No such file or directory
    $ sudo ln -s libcrypto.so.1.0.0 /usr/lib/i386-linux-gnu/libcrypto.so.4

You also need to update your `~/.iourc` file. You may have downloaded
a `keygen.py` script just for that.
