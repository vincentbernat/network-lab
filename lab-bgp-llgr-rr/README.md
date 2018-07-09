# BGP LLGR with route reflectors

This is a derivative of `lab-bgp-llgr` with route reflectors. We can
validate stale routes are correctly transmitted to BGP LLGR-aware RR
and correctly recognized to the other end:

On S1:

    # ddos sw11

On C4:

    # sudo birdc6 show route
    2001:db8:ff::3/128 via 2001:db8:1::3 on eth0.21 [RR21 16:33:04 from 2001:db8:1::f2] * (100) [i]
                       via 2001:db8:2::3 on eth0.22 [RR22 16:33:05 from 2001:db8:2::f2] (100) [i]
    2001:db8:ff::2/128 via 2001:db8:2::2 on eth0.22 [RR22 16:33:06 from 2001:db8:2::f2] * (100) [i]
                       via 2001:db8:1::2 on eth0.21 [RR21 16:37:35 from 2001:db8:1::f2] (100s) [i]
    2001:db8:ff::1/128 via 2001:db8:2::1 on eth0.22 [RR22 16:33:06 from 2001:db8:2::f2] * (100) [i]
                       via 2001:db8:1::1 on eth0.21 [RR21 16:37:35 from 2001:db8:1::f2] (100s) [i]
    2001:db8:ff::4/128 dev lo [direct1 16:33:02] * (240)
    2001:db8:ff::ca/128 via 2001:db8:2::1 on eth0.22 [RR22 16:33:06 from 2001:db8:2::f2] * (100) [i]
                       via 2001:db8:1::1 on eth0.21 [RR21 16:37:35 from 2001:db8:1::f2] (100s) [i]
                       via 2001:db8:1::2 on eth0.21 [RR21 16:37:35 from 2001:db8:1::f2] (100s) [i]
                       via 2001:db8:2::2 on eth0.22 [RR22 16:33:06 from 2001:db8:2::f2] (100) [i]
                       via 2001:db8:2::3 on eth0.22 [RR22 16:33:05 from 2001:db8:2::f2] (100) [i]
                       via 2001:db8:1::3 on eth0.21 [RR21 16:33:04 from 2001:db8:1::f2] (100) [i]

For BIRD, the well-known community used is 65535:6.
