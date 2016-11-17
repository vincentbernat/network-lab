Lab with Juniper Firefly Perimeter
==================================

Firefly Perimeter is a virtual SRX appliance.

Lab
---

This lab is quite simple. Two Juniper SRX and two Linux running BIRD
are plugged on the same virtual switch and establish OSPF adjacencies
between them (with BFD for faster convergence times).

    root@SRX2> show ospf neighbor    
    Address          Interface              State     ID               Pri  Dead
    192.0.2.3        ge-0/0/1.0             2Way      3.3.3.3          128    39
    192.0.2.2        ge-0/0/1.0             Full      2.2.2.2            1    31
    192.0.2.1        ge-0/0/1.0             Full      1.1.1.1            1    29
    
    root@SRX2> show ospf route       
    Topology default Route Table:
    
    Prefix             Path  Route      NH       Metric NextHop       Nexthop      
                       Type  Type       Type            Interface     Address/LSP
    1.1.1.1            Intra Router     IP            1 ge-0/0/1.0    192.0.2.1
    2.2.2.2            Intra Router     IP            1 ge-0/0/1.0    192.0.2.2
    3.3.3.3            Intra Router     IP            1 ge-0/0/1.0    192.0.2.3
    192.0.2.0/24       Intra Network    IP            1 ge-0/0/1.0
    198.51.100.101/32  Intra Network    IP            1 ge-0/0/1.0    192.0.2.1
    198.51.100.102/32  Intra Network    IP            1 ge-0/0/1.0    192.0.2.2
    198.51.100.104/32  Intra Network    IP            0 lo0.0
    
    root@SRX2> show bfd session      
                                                      Detect   Transmit
    Address                  State     Interface      Time     Interval  Multiplier
    192.0.2.1                Up        ge-0/0/1.0     1.000     0.200        5   
    192.0.2.2                Up        ge-0/0/1.0     1.000     0.200        5   
    192.0.2.3                Up        ge-0/0/1.0     1.000     0.200        5   
    
    3 sessions, 3 clients
    Cumulative transmit rate 15.0 pps, cumulative receive rate 15.0 pps

The transmit interval for BFD is 200 ms but it can be reduced on real hardware.

Download
--------

You can download it from [Juniper website][]. Choose the KVM image and
extract it by using the `-x` flag. This should get you three
files. One of them is the image. Put it in the `images/` directory and
name it `junos-firefly.img`.

[Juniper website]: http://www.juniper.net/us/en/products-services/security/firefly-perimeter/#evaluation

Autoconfiguration
-----------------

When booting, the Juniper are starting from the factory default
configuration and we exploit the auto installation feature to download
the correct configuration through TFTP. To troubleshoot, look at what
is happening in `/var/log/autod` on each Juniper.

This needs a modified version of QEMU to work! Otherwise, have a look
at commit aac8dccbae8a which contains a version without this
restriction.

The patch to apply to QEMU is the following:

    --- a/slirp/tftp.c
    +++ b/slirp/tftp.c
    @@ -326,13 +326,15 @@ static void tftp_handle_rrq(Slirp *slirp, struct sockaddr_storage *srcsas,
         return;
       }
     
    -  if (strcasecmp(&tp->x.tp_buf[k], "octet") != 0) {
    +  if (strcasecmp(&tp->x.tp_buf[k], "octet") == 0) {
    +      k += 6;
    +  } else if (strcasecmp(&tp->x.tp_buf[k], "netascii") == 0) {
    +      k += 9;
    +  } else {
           tftp_send_error(spt, 4, "Unsupported transfer mode", tp);
           return;
       }
     
    -  k += 6; /* skipping octet */
    -
       /* do sanity checks on the filename */
       if (!strncmp(req_fname, "../", 3) ||
           req_fname[strlen(req_fname) - 1] == '/' ||
