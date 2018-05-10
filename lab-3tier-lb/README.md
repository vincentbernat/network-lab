# 3-tier load balancer

This is an example of a 3-tier loadbalancer using ECMP routing (BGP),
IPVS L4 load-balancing with consistent hashing and stateful L7
load-balancing (with HAProxy). Direct return is used to bypass the L4
load-balancing layer. To accomodate cloud-environments or fully-routed
environments, IPIP tunnels are used.

From the `users` VM, we can issue a curl command to test if everything
works as expected:

    $ curl --interface 203.0.113.184 198.51.100.1
    S3
    $ curl --interface 203.0.113.164 198.51.100.1
    S4

To enter a namespace from V, one can use:

    # nsenter --all -t $(pidof sleep) -r -w

There is also a fourth tier with DNS. You can query
`{www,www1,www2}.example.org`.

## Stateless to stateful

The first tier (ECMP load-balancing) is mostly stateless: while
packets from a given flow are sent to the same destination, a single
change/failure may totally change the distribution. There is also no
guarantee that ICMP exceptions will end on the same destination than
its associated flow. In a cloud environment, your hoster may provide
this layer for you (as an anycast IP or a L4 load-balancer without
consistent hashing).

On the other hand/end, the last tier (L7 load-balancing) is stateful:
it can only handle packets from a known and existing connection or
from a new connection (SYN packet). Any packets from an unknown
connection will be rejected (with a TCP RST most of the time), closing
the client connection. An ICMP exception should also be received by
the same server handling the associated TCP flow.

This explains why we need the second tier (L4 load-balancing with
IPVS): it is the glue between the two worlds. A first approach would
be to synchronize state between all servers. IPVS can only work in
active/backup mode when syncing state. This is not scalable. The
approach chosen here is to use a consistent hash algorithm which will
minimize changes when a disruption happen. Some TCP connections may
get broken but most of them should survive.

There different disruption possibles:

 - A change in the ECMP routing may reroute packets to another IPVS
   server. This is handled by the consistent hashing algorithm: all
   IPVS servers have mostly the same view and will schedule a given
   packet in the same way. Almost no disruption should happen when
   such a change happens.

 - A change in the number of IPVS servers should behave in the same
   way: no disruption expected.

 - If an HAProxy server disappear, all the connections it was handling
   are lost. Not much we can do. However, thanks to the use of
   consistent hashing, connections to other HAProxy servers should
   stay mostly untouched.

 - If an HAProxy server is added, the disruption can be quite
   minimal. The IPVS servers are not totally stateless. Existing
   connections are not re-scheduled and continue to be sent to the
   appropriate HAProxy server. Only new connections will be scheduled
   to the new one. However, the state is local to a given IPVS
   server. If another disruption happens (change in the number of IPVS
   servers, change in ECMP routing), some connections may end on the
   "wrong" IPVS server and rescheduled on the "wrong" HAProxy
   server. They will get reset. This is the weakness of such a setup
   and it is believed such a scenario should not happen often.

 - If a backend server is added, no impact should happen: HAProxy
   servers are stateful.

 - If a backend server is lost, only the connections attached to it
   are lost (not much we can do).

The last tier is optional. You may want to not have it if you don't
need fancy L7 load-balancing features. However, it also brings
stability to the whole structure. Since adding/removing servers from
this tier may trigger a more important impact if there is another
change in the previous tiers, it is not a good idea to directly have
the backend servers as last tier if you want to add/them remove them
often (blue/green deployment, scale-up/scale-down).

## Maintenance

There are various parts that could be put on maintenance:

 - L4 load-balancer can be put in maintenance by creating
   `/etc/lb/disable`. The service IP addresses will stop being
   advertised.
 - L7 load-balancer can be put in maintenance by writing `down` in
   `/etc/lb/agent-check`. This will disable a fake server and make the
   monitored URI fail.
 - Servers can be put in maintenance by writing `drain` or `down` in
   `/etc/lb/agent-check`. See the [agent-check][] directive for more
   details. Alternatively, from HAProxy, the socket can also be used
   for this purpose.

Agent checks are served by `socat`. This may seem a little brittle but
it serves the purpose for this demonstration.

[agent-check]: https://cbonte.github.io/haproxy-dconv/1.8/configuration.html#agent-check
