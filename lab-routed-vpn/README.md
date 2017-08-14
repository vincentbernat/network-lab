# Route-based VPN with Linux

Route-based VPN are easier to handle than policy-based VPN in several cases:

 - several VPN serving the same networks
 - route-learning through BGP

This lab shows both scenarios. The most important point are shown in
the diagram. Notably, each VPN has one public IP address (203.0.113.X)
and can reach a private network (192.168.1.X/26). The goal is to setup
the VPN such that each private network can be routed to another site.

With policy-based VPN, we could have paired the VPNs (V1-1 connects to
V2-1 and V3-1) and statically establish policy for private
networks. However, If both V1-1 and V2-2 are out of order, AS65001
cannot speak to AS65002 anymore. Moreover, policies need to be
declared in addition to routing rules.

With route-based VPN, we can establish a mesh between VPNs and
distribute routes with BGP.
