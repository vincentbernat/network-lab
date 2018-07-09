# BGP LLGR with route reflectors

This is a derivative of `lab-bgp-llgr` with route reflectors. We can
validate stale routes are correctly transmitted to BGP LLGR-aware RR
and correctly recognized to the other end:

On S1:

    # ddos eth4.11

On C4:

    # birdc6 show route
    BIRD 1.6.4 ready.
    2001:db8:ff::3/128 via 2001:db8:1::3 on eth0.21 [RR21 17:26:10 from 2001:db8:1::f2] * (100) [i]
                       via 2001:db8:2::3 on eth0.22 [RR22 17:26:10 from 2001:db8:2::f2] (100) [i]
    2001:db8:ff::2/128 via 2001:db8:1::2 on eth0.21 [RR21 17:26:12 from 2001:db8:1::f2] * (100) [i]
                       via 2001:db8:2::2 on eth0.22 [RR22 17:26:11 from 2001:db8:2::f2] (100) [i]
    2001:db8:ff::1/128 via 2001:db8:2::1 on eth0.22 [RR22 17:26:11 from 2001:db8:2::f2] * (100) [i]
                       via 2001:db8:1::1 on eth0.21 [RR21 17:30:20 from 2001:db8:1::f2] (100s) [i]
    2001:db8:ff::4/128 dev lo [direct1 17:21:30] * (240)
    2001:db8:ff::ca/128 via 2001:db8:2::1 on eth0.22 [RR22 17:26:11 from 2001:db8:2::f2] * (100) [i]
                       via 2001:db8:1::1 on eth0.21 [RR21 17:30:20 from 2001:db8:1::f2] (100s) [i]
                       via 2001:db8:1::2 on eth0.21 [RR21 17:26:12 from 2001:db8:1::f2] (100) [i]
                       via 2001:db8:2::2 on eth0.22 [RR22 17:26:11 from 2001:db8:2::f2] (100) [i]
                       via 2001:db8:1::3 on eth0.21 [RR21 17:26:10 from 2001:db8:1::f2] (100) [i]
                       via 2001:db8:2::3 on eth0.22 [RR22 17:26:10 from 2001:db8:2::f2] (100) [i]

For BIRD, the well-known community used is 65535:6. Juniper is using this one as well.
