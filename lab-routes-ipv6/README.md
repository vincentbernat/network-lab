# Experiments with Linux IPv6 FIB6

## About source routing

With `CONFIG_IPV6_SUBTREES`, you can insert source-specific routing
entries. The lookup is first done on the most specific destination and
if the entry has source-specific nodes, then the most specific source
is used. This can be done with `ip route` (undocumented):

    ip -6 route add 2001:db8:1::/64 from 2001:db8:3::/64 via fe80::1 dev eth0

This is used by protocols like Babel. This feature is present since
Linux 2.1.30! It has been dead code until 2.6.19 (4e96c2b4180a). For
Babel, it became usable with 3.11 (3e3be275851b).

We totally ignore this part.

## About caching

For IPv4, the (external) route cache has been removed in 3.6. Each
next hop can have a list of "exceptions" (like MTU) attached to it. In
IPv6, the cached entries are directly put in the routing tree.

Before Linux 4.2, any lookup would create a cache entry. This was
massively inefficient. This was fixed in 45e4fd26683c. Cache entries
are only created when a PMTU exception occurs or when there is a
redirect.

Since there is a cache, there are routines to trim it up:

    net.ipv6.route.gc_elasticity = 9
    net.ipv6.route.gc_interval = 30
    net.ipv6.route.gc_min_interval = 0
    net.ipv6.route.gc_min_interval_ms = 500
    net.ipv6.route.gc_thresh = 1024
    net.ipv6.route.gc_timeout = 60

You can display the cache with `ip -6 route show cache`.

For a router, we would assume that those entries should not exist
(PMTU handling is done by hosts).

## About memory

While IPv4 gives easy access to memory use, this is not the case for
IPv6. However, we can look at the SLAB cache for `ip6_dst_cache` and
`fib6_nodes`. See:

    $ sed -ne 1,2p -e '/^ip6/p' -e '/^fib6/p' /proc/slabinfo
    slabinfo - version: 2.1
    # name            <active_objs> <num_objs> <objsize> <objperslab> <pagesperslab> : tunables <limit> <batchcount> <sharedfactor> : slabdata <active_slabs> <num_slabs> <sharedavail>
    ip6-frags              0      0    200   20    1 : tunables  120   60    0 : slabdata      0      0      0
    fib6_nodes            90    126     64   63    1 : tunables  120   60    0 : slabdata      2      2      0
    ip6_dst_cache         97    130    384   10    1 : tunables   54   27    0 : slabdata     13     13      0
    ip6_mrt_cache          0      0    192   21    1 : tunables  120   60    0 : slabdata      0      0      0

So, we have 126*64 + 130*384 bytes in this example.

## About statistics

Some very light statistics are kept for IPv6:

    $ cat /proc/net/rt6_stats
    0000 0025 0000 003b 0000 0001 0000

In order:

 - always 0
 - number of nodes
 - always 0
 - number of routes
 - always 0
 - number of cached entries
 - number of removed routes

Not really exciting.

## Lookup functions

For IPv4, the "main" lookup function is `fib_lookup()`. For IPv6, the
equivalent seems to be `ip6_route_lookup()`. It will use IP rules to
select a table and call `fib6_lookup()` to do the actual
lookup. However, this function is only used by Netfilter for
reverse-path filtering (commit ea6e574e3477).

There is also `rt6_lookup()`. However, it is also used only for
administrative purpose (when installing a route, when checking the
validity of something route-related). Never for forwarding.

The next ones are `ip6_route_input_lookup()` and
`ip6_route_output_flags()` (the second one is called for
locally-generated packets). They end up calling `ip6_pol_route()`
which calls `fib6_lookup()` and arrange to get a per-CPU copy of the
DST entry.
