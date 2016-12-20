Lab with Juniper vMX
====================

Almost the same as the lab with vSRX. We only use one vMX as they are
quite memory heavy. This has been tested with the 16.1 version. You
need to extract the three images from the tarball
(`junos-vmx-x86-74*.qcow2`, `vmxhdd.img` and `vFPC*.img`). Convert the
later to the QCOW format:

    $ qemu-img convert -c -O qcow2 vFPC-20160617.img vFPC-20160617.qcow2
    
Then, create the appropriate symlinks:

    $ ln -s vmxhdd.img junos-vmx-re-hdd.img
    $ ln -s junos-vmx-x86*.qcow2 junos-vmx-re.img
    $ ln -s vFPC*.qcow2 junos-vmx-pfe.img
    
There are several names for the same thing:

 - RE, vRE, vCP (control plane)
 - FPC, PFE, vFP, vPFE (data plane)

The password for the RE is `Juniper`. The password for the PFE is `root`.

The
[license](http://www.juniper.net/us/en/dm/free-vmx-trial/E421992502.txt) needs
to be installed:

     root@vMX> request system license add terminal
     [Type ^D at a new line to end input,
      enter blank line between each license key]
     E421992502 aeaqic aiagii ipabsc idycyi giydcn jrgazd
                adapoz gvqlkk ovxgs4 dfojcx mylma4 ukgjwr
                zccblo iwdzky 55ejop gj7pxv 3ftbxo dz3d3c
                qgwi7f aambfm w6i3ai 4aadc2 mpi
     E421992502: successfully added
     add license complete (no errors)

Then, the configuration needs to be committed again (otherwise, the
PFE won't notice the new license). vMX should now come with a
perpetual base license, but this doesn't seem to be the case for the
16.1 version (release notes say this should be the case for 15.1F6).

Lab
---

This lab is quite simple. One Juniper vMX and two Linux running BIRD
are plugged on the same virtual switch and establish OSPF adjacencies
between them (with BFD for faster convergence times).

    root@vMX> show ospf neighbor
    Address          Interface              State     ID               Pri  Dead
    192.0.2.2        ge-0/0/0.0             Full      2.2.2.2            1    38
    192.0.2.1        ge-0/0/0.0             Full      1.1.1.1            1    38
    
    root@vMX> show ospf route
    Topology default Route Table:
    
    Prefix             Path  Route      NH       Metric NextHop       Nexthop
                       Type  Type       Type            Interface     Address/LSP
    1.1.1.1            Intra Router     IP            1 ge-0/0/0.0    192.0.2.1
    2.2.2.2            Intra Router     IP            1 ge-0/0/0.0    192.0.2.2
    192.0.2.0/24       Intra Network    IP            1 ge-0/0/0.0
    198.51.100.101/32  Intra Network    IP            1 ge-0/0/0.0    192.0.2.1
    198.51.100.102/32  Intra Network    IP            1 ge-0/0/0.0    192.0.2.2
    198.51.100.103/32  Intra Network    IP            0 lo0.0
    
    root@vMX> show bfd session
                                                      Detect   Transmit
    Address                  State     Interface      Time     Interval  Multiplier
    192.0.2.1                Up        ge-0/0/0.0     1.000     0.200        5
    192.0.2.2                Up        ge-0/0/0.0     1.000     0.200        5
    
    2 sessions, 2 clients
    Cumulative transmit rate 10.0 pps, cumulative receive rate 10.0 pps

The transmit interval for BFD is 200 ms but it can be reduced on real hardware.
