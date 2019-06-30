# Bonding GARP experiments

We experiment with `peer_notify_delay`. On H1, run:

    ip link set down dev eth0
    ip link set up dev eth0

On H2, you should noticed with `tcpdump`:

    tcpdump -pni eth0
    tcpdump: verbose output suppressed, use -v or -vv for full protocol decode
    listening on eth0, link-type EN10MB (Ethernet), capture size 262144 bytes
    18:36:06.792651 IP6 fe80::5254:33ff:fe00:3 > ff02::2: ICMP6, router solicitation, length 16
    18:36:09.099460 ARP, Request who-has 203.0.113.10 tell 203.0.113.10, length 28
    18:36:09.099475 IP6 fe80::5254:33ff:fe00:1 > ff02::1: ICMP6, neighbor advertisement, tgt is fe80::5254:33ff:fe00:1, length 32
    18:36:09.202912 ARP, Request who-has 203.0.113.10 tell 203.0.113.10, length 28
    18:36:09.202925 IP6 fe80::5254:33ff:fe00:1 > ff02::1: ICMP6, neighbor advertisement, tgt is fe80::5254:33ff:fe00:1, length 32
    18:36:09.722939 ARP, Request who-has 203.0.113.10 tell 203.0.113.10, length 28
    18:36:09.722954 IP6 fe80::5254:33ff:fe00:1 > ff02::1: ICMP6, neighbor advertisement, tgt is fe80::5254:33ff:fe00:1, length 32
    18:36:10.242825 ARP, Request who-has 203.0.113.10 tell 203.0.113.10, length 28
    18:36:10.242838 IP6 fe80::5254:33ff:fe00:1 > ff02::1: ICMP6, neighbor advertisement, tgt is fe80::5254:33ff:fe00:1, length 32
    18:36:10.762699 ARP, Request who-has 203.0.113.10 tell 203.0.113.10, length 28
    18:36:10.762707 IP6 fe80::5254:33ff:fe00:1 > ff02::1: ICMP6, neighbor advertisement, tgt is fe80::5254:33ff:fe00:1, length 32
    18:36:11.282886 ARP, Request who-has 203.0.113.10 tell 203.0.113.10, length 28
    18:36:11.282899 IP6 fe80::5254:33ff:fe00:1 > ff02::1: ICMP6, neighbor advertisement, tgt is fe80::5254:33ff:fe00:1, length 32

See http://patchwork.ozlabs.org/patch/1124972/ for the patch adding
`peer_notif_delay`.
