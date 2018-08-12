# Experiments with Linux IPv6 FIB6

## Source routing

With `CONFIG_IPV6_SUBTREES`, you can insert source-specific routing
entries. The lookup is first done on the most specific destination and
if the entry has source-specific nodes, then the most specific source
is used. This can be done with `ip route` (undocumented):

    ip -6 route add 2001:db8:1::/64 from 2001:db8:3::/64 via fe80::1 dev eth0

This is used by protocols like Babel. This feature is present since
Linux 2.1.30! It has been dead code until 2.6.19 (4e96c2b4180a). For
Babel, it became usable with 3.11 (3e3be275851b).

We totally ignore this part.

## Caching

For IPv4, the (external) route cache has been removed in 3.6. Each
next hop can have a list of "exceptions" (like MTU) attached to it. In
IPv6, the cache entries are directly put in the routing tree.

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

## Memory

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

So, we have 126×64 + 130×384 bytes in this example.

However, after 4.2 (commit d52d3997f843), there is also per-CPU copies
allocated. They are allocated like normal entries but some pointers to
manage them (`rt6i_pcpu`) are allocated directly, so it's a bit
difficult to track them. For each `struct rt6_info`, we have one
additional pointer (8 bytes) allocated per CPU. This extends the size
of the object from 384 to 384+8×n. This matches my empiric
observations. Cache entries may not have those additional pointers
(dunno why). We assume they are rare enough to not warrant accounting
for them.

In 4.12, a `struct rt6_info` is 336 bytes and a `struct fib6_node` is
48 bytes.

## Statistics

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

## Structures

The main structure is `struct fib6_node`:

    struct fib6_node {
        struct fib6_node    *parent;
        struct fib6_node    *left;
        struct fib6_node    *right;
    #ifdef CONFIG_IPV6_SUBTREES
        struct fib6_node    *subtree;
    #endif
        struct rt6_info     *leaf;
    
        __u16           fn_bit;     /* bit key */
        __u16           fn_flags;
        int         fn_sernum;
        struct rt6_info     *rr_ptr;
    };

`parent`, `left`, `right` and `fn_bit` implements the radix trie. A
leaf node is one where `fn_flags` has the bit `RTN_INFO` set. When an
appropriate entry is found, the possible routes associated to the
prefix are present in `leaf`, a `struct rt6_info`:

    struct rt6_info {
        struct dst_entry        dst;
    
        /*
         * Tail elements of dst_entry (__refcnt etc.)
         * and these elements (rarely used in hot path) are in
         * the same cache line.
         */
        struct fib6_table       *rt6i_table;
        struct fib6_node        *rt6i_node;
    
        struct in6_addr         rt6i_gateway;
    
        /* Multipath routes:
         * siblings is a list of rt6_info that have the the same metric/weight,
         * destination, but not the same gateway. nsiblings is just a cache
         * to speed up lookup.
         */
        struct list_head        rt6i_siblings;
        unsigned int            rt6i_nsiblings;
    
        atomic_t            rt6i_ref;
    
        /* These are in a separate cache line. */
        struct rt6key           rt6i_dst ____cacheline_aligned_in_smp;
        u32             rt6i_flags;
        struct rt6key           rt6i_src;
        struct rt6key           rt6i_prefsrc;
    
        struct list_head        rt6i_uncached;
        struct uncached_list        *rt6i_uncached_list;
    
        struct inet6_dev        *rt6i_idev;
        struct rt6_info * __percpu  *rt6i_pcpu;
    
        u32             rt6i_metric;
        u32             rt6i_pmtu;
        /* more non-fragment space at head required */
        unsigned short          rt6i_nfheader_len;
        u8              rt6i_protocol;
    };

For ECMP routes, `rt6i_siblings` links the different routes. For
non-ECMP routes, they are linked through `rt6_next` field of `dst`.

## Performance

Even with an almost empty routing table, the performance of IPv6
lookup is worse than its IPv4 counter-part. Use of `perf record -F
9999 --all-kernel -g -- cat /sys/kernel/kbench/run` can help to
pinpoint the problem:

 - Policy rules are always evaluated, even when they are untouched.
   This is not the case for IPv4. With an almost empty routing table,
   30% of the time is spent in evaluating those rules. This is fixed
   in 4.14.

 - Main and local tables are not merged. A first match is done against
   local, then a second one is done for main. About 10% of the time is
   lost in this check (always with almost empty tables).

 - Locking is done with a read lock, which is more expensive than the
   RCU mechanism used for IPv4. About 10% of the time is lost here.

This can be observed with some flamegraphs: `v6-flamegraph.svg` and
`v4-flamegraph.svg` (unmerged routing tables, IP rules evaluated,
otherwise all time is spent in `fib_table_lookup()`). In both cases,
routing table only contain a default route. IPv6 just needs to get the
same optimization than for IPv4 with respect to the routing
tables. Note that in the IPv4 case, there are 2 calls to
fib_table_lookup() (local, main) but as they are on the same level, we
only see one.

A small test with 40k routes show that compiling-out table support
saves about 100ns. Compiling-out subtree support only saves less than
5%. This roughly matches the flamegraph (75% time spent in
`ip6_pol_route_output` for a total of 300ns minus 5%).

As for the evolution of the performance:

 - In 4.2, huge progression, notably with 4b32b5ad31a6 but also
   45e4fd26683c. On SMP systems, it is likely that d52d3997f843 would
   have helped too. All this work is from Martin KaFai Lau (Facebook).

 - In 3.9, a small regression, partly due to 887c95cc1da5 (quite
   surprising, maybe cache-related but just adding back the field in
   `struct rt6_info` doesn't help). I wasn't able to pinpoint a second
   commit, but I suppose the whole neighbor removal is to
   "blame". This could be investigated a bit more since there are not
   many commits.

 - In 3.1, an important regression due to 21efcfa0ff27 which
   effectively ensure metrics are allocated for each cache entry.

### Fairness of comparison with IPv4

With IPv4, the `fib_lookup()` function is evaluated. For IPv6, we use
`ip6_route_output()`. Is it fairer to use the `fib6_lookup()`
function? Let's check the difference between the two functions (on a
4.17 kernel).

Here is `ip6_route_output()`, including the call to `fib6_lookup()`:

    funcgraph_entry:                   |  ip6_route_output_flags() {
    funcgraph_entry:        0.040 us   |    __ipv6_addr_type();
    funcgraph_entry:        0.035 us   |    __ipv6_addr_type();
    funcgraph_entry:                   |    fib6_rule_lookup() {
    funcgraph_entry:                   |      ip6_pol_route_output() {
    funcgraph_entry:                   |        ip6_pol_route() {
    funcgraph_entry:                   |          fib6_lookup() {
    funcgraph_entry:        0.595 us   |            fib6_lookup_1();
    funcgraph_exit:         0.921 us   |          }
    funcgraph_entry:        0.038 us   |          fib6_backtrack();
    funcgraph_entry:        0.033 us   |          fib6_backtrack();
    funcgraph_entry:                   |          rt6_find_cached_rt() {
    funcgraph_entry:        0.103 us   |            __rt6_find_exception_rcu();
    funcgraph_exit:         0.484 us   |          }
    funcgraph_exit:         3.236 us   |        }
    funcgraph_exit:         3.541 us   |      }
    funcgraph_entry:        0.033 us   |      dst_release();
    funcgraph_entry:                   |      ip6_pol_route_output() {
    funcgraph_entry:                   |        ip6_pol_route() {
    funcgraph_entry:                   |          fib6_lookup() {
    funcgraph_entry:        0.308 us   |            fib6_lookup_1();
    funcgraph_exit:         0.554 us   |          }
    funcgraph_entry:                   |          find_match() {
    funcgraph_entry:        0.108 us   |            rt6_check_expired();
    funcgraph_entry:                   |            rt6_score_route() {
    funcgraph_entry:        0.035 us   |              _raw_read_lock();
    funcgraph_entry:        0.042 us   |              __local_bh_enable_ip();
    funcgraph_exit:         1.119 us   |            }
    funcgraph_entry:        0.019 us   |            __local_bh_enable_ip();
    funcgraph_exit:         2.183 us   |          }
    funcgraph_entry:                   |          rt6_find_cached_rt() {
    funcgraph_entry:        0.028 us   |            __rt6_find_exception_rcu();
    funcgraph_exit:         0.264 us   |          }
    funcgraph_entry:        0.260 us   |          ip6_hold_safe();
    funcgraph_entry:        0.027 us   |          __local_bh_enable_ip();
    funcgraph_exit:         4.991 us   |        }
    funcgraph_exit:         5.220 us   |      }
    funcgraph_exit:         9.628 us   |    }
    funcgraph_exit:       + 10.866 us  |  }

`fib_lookup()` is an inline function, it's more difficult to trace. If
it custom IP rules are used, it just jumps to `__fib_lookup()` which
can be traced. Otherwise, it takes an RCU read lock and jumps to
`fib_table_lookup()`, which is too small to trace. So, we seem to be
unfair to IPv6. Looking at the stacktrace when calling
`fib_table_lookup()`, we can notice a call to
`ip_route_output_flow()`. Let's trace this one:

    funcgraph_entry:                   |  ip_route_output_flow() {
    funcgraph_entry:                   |    ip_route_output_key_hash() {
    funcgraph_entry:                   |      ip_route_output_key_hash_rcu() {
    funcgraph_entry:                   |        __ip_dev_find() {
    funcgraph_entry:        0.148 us   |          inet_lookup_ifaddr_rcu();
    funcgraph_exit:         0.453 us   |        }
    funcgraph_entry:        0.067 us   |        fib_table_lookup();
    funcgraph_entry:        0.039 us   |        find_exception();
    funcgraph_exit:         1.293 us   |      }
    funcgraph_exit:         1.538 us   |    }
    funcgraph_entry:                   |    xfrm_lookup_route() {
    funcgraph_entry:        0.086 us   |      xfrm_lookup();
    funcgraph_exit:         0.324 us   |    }
    funcgraph_exit:         2.460 us   |  }

This time, we make the IPv4 function work more than the IPv6 one.
Let's use `ip6_dst_lookup_flow()` for IPv6.

    funcgraph_entry:                   |  ip6_dst_lookup_flow() {
    funcgraph_entry:                   |    ip6_dst_lookup_tail() {
    funcgraph_entry:                   |      ip6_route_output_flags() {
    funcgraph_entry:        0.141 us   |        __ipv6_addr_type();
    funcgraph_entry:        0.099 us   |        __ipv6_addr_type();
    funcgraph_entry:                   |        fib6_rule_lookup() {
    funcgraph_entry:                   |          ip6_pol_route_output() {
    funcgraph_entry:                   |            ip6_pol_route() {
    funcgraph_entry:                   |              fib6_lookup() {
    funcgraph_entry:        1.160 us   |                fib6_lookup_1();
    funcgraph_exit:         2.195 us   |              }
    funcgraph_entry:        0.120 us   |              fib6_backtrack();
    funcgraph_entry:        0.094 us   |              fib6_backtrack();
    funcgraph_entry:                   |              rt6_find_cached_rt() {
    funcgraph_entry:        0.124 us   |                __rt6_find_exception_rcu();
    funcgraph_exit:         1.023 us   |              }
    funcgraph_exit:         7.437 us   |            }
    funcgraph_exit:         8.288 us   |          }
    funcgraph_entry:        0.094 us   |          dst_release();
    funcgraph_entry:                   |          ip6_pol_route_output() {
    funcgraph_entry:                   |            ip6_pol_route() {
    funcgraph_entry:                   |              fib6_lookup() {
    funcgraph_entry:        0.474 us   |                fib6_lookup_1();
    funcgraph_exit:         1.259 us   |              }
    funcgraph_entry:                   |              find_match() {
    funcgraph_entry:        0.226 us   |                rt6_check_expired();
    funcgraph_entry:                   |                rt6_score_route() {
    funcgraph_entry:        0.103 us   |                  _raw_read_lock();
    funcgraph_entry:        0.164 us   |                  __local_bh_enable_ip();
    funcgraph_exit:         2.838 us   |                }
    funcgraph_entry:        0.080 us   |                __local_bh_enable_ip();
    funcgraph_exit:         5.731 us   |              }
    funcgraph_entry:                   |              rt6_find_cached_rt() {
    funcgraph_entry:        0.080 us   |                __rt6_find_exception_rcu();
    funcgraph_exit:         0.892 us   |              }
    funcgraph_entry:        0.466 us   |              ip6_hold_safe();
    funcgraph_entry:        0.084 us   |              __local_bh_enable_ip();
    funcgraph_exit:       + 12.687 us  |            }
    funcgraph_exit:       + 13.382 us  |          }
    funcgraph_exit:       + 24.104 us  |        }
    funcgraph_exit:       + 26.809 us  |      }
    funcgraph_entry:                   |      ipv6_dev_get_saddr() {
    funcgraph_entry:        0.080 us   |        __ipv6_addr_type();
    funcgraph_entry:                   |        ipv6_addr_label() {
    funcgraph_entry:        1.354 us   |          __ipv6_addr_label();
    funcgraph_exit:         2.275 us   |        }
    funcgraph_entry:        0.259 us   |        l3mdev_master_ifindex_rcu();
    funcgraph_entry:                   |        __ipv6_dev_get_saddr() {
    funcgraph_entry:        0.121 us   |          __ipv6_addr_type();
    funcgraph_entry:        0.352 us   |          ipv6_get_saddr_eval();
    funcgraph_entry:        0.150 us   |          ipv6_get_saddr_eval();
    funcgraph_exit:         3.173 us   |        }
    funcgraph_entry:        0.073 us   |        l3mdev_master_ifindex_rcu();
    funcgraph_entry:                   |        __ipv6_dev_get_saddr() {
    funcgraph_entry:        0.098 us   |          __ipv6_addr_type();
    funcgraph_entry:        0.113 us   |          ipv6_get_saddr_eval();
    funcgraph_entry:        0.138 us   |          ipv6_get_saddr_eval();
    funcgraph_entry:        0.145 us   |          ipv6_get_saddr_eval();
    funcgraph_entry:        0.109 us   |          ipv6_get_saddr_eval();
    funcgraph_entry:        0.164 us   |          ipv6_get_saddr_eval();
    funcgraph_entry:        0.146 us   |          ipv6_get_saddr_eval();
    funcgraph_entry:        0.127 us   |          __ipv6_addr_type();
    funcgraph_entry:        0.113 us   |          ipv6_get_saddr_eval();
    funcgraph_entry:        0.120 us   |          ipv6_get_saddr_eval();
    funcgraph_entry:        0.110 us   |          ipv6_get_saddr_eval();
    [interrupt]
    funcgraph_entry:        0.124 us   |          ipv6_get_saddr_eval();
    funcgraph_entry:        0.124 us   |          ipv6_get_saddr_eval();
    funcgraph_entry:        0.176 us   |          ipv6_get_saddr_eval();
    funcgraph_exit:       + 75.621 us  |        }
    funcgraph_exit:       + 86.953 us  |      }
    funcgraph_entry:        0.080 us   |      __local_bh_enable_ip();
    funcgraph_exit:       ! 116.541 us |    }
    funcgraph_entry:                   |    xfrm_lookup_route() {
    funcgraph_entry:        0.385 us   |      xfrm_lookup();
    funcgraph_exit:         1.302 us   |    }
    funcgraph_exit:       ! 119.913 us |  }

For some reason, we cannot record a single trace without an interrupt.
May it because the function is too slow?

It's hard to find an exact function, but looking for a function higher
in the stack is not to IPv6 advantage.

