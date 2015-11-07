RTBH Filtering
==============

This is a very simple lab to test RTBH filtering with Bird. There are
two clients (*C1* and *C2*):

 - *C1* (AS 65001) is pushing routes to blackhole from its edge router using a
   special community to its peer. The route is tagged with a special
   community.
 - *C2* (AS 65002) is pusging routes to blackhole from a dedicated
   server using to a dedicated RTBH server. No special community is
   needed.

The edge router for the provider is *P* (AS 65000). The RTBH server is
*RTBH*.
