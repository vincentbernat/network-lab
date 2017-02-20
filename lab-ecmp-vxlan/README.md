# ECMP and VXLAN/multicast

This is a small experiment to make VXLAN with multicast work over an
IP network with ECMP paths. Notably, ECMP paths start directly from
the host. We expect traffic to be able to use both interfaces.

With a naive setup, we have to specify the output interface. Instead,
we specify to use `lo` and let pimd figure out how to build the RP
tree. This needs pimd 2.4.0 or more recent (see
https://github.com/troglobit/pimd/pull/89,
https://github.com/troglobit/pimd/pull/91).

In the current setup, multicast is not using diverse paths as the tie
breaker used is the highest address. However, most of the VXLAN based
traffic will be routed over multiple paths (diversity is OK in this
case as the source port is diverse enough). If diversity was
important, Linux could maybe teached to use s-g-hash as a method for
load splitting. See this documentation from Cisco:
http://www.cisco.com/c/en/us/td/docs/ios/12_4t/ip_mcast/configuration/guide/mctlsplt.html

The lab is not quite functional, notably:

 - PIM assert mechanism seem to be broken and duplicate packets can be
   delivered.
 - Topology change seems to not be handled correctly by pimd. Once
   routing is stable, pimd may need to be restarted.
