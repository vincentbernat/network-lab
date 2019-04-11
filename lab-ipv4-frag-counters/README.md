# Fragmentation testing

H1 fetches a big file from H2 (`curl 203.0.113.130/10G > /dev/null`).
PMTU is somehow disabled for H1 and H2. R1/R2 are connected through a
link with a reduced MTU and therefore, R2 is forced to fragment.

On H1, we check that reassembly occurs:

    Ip:
        Forwarding: 2
        107889 total packets received
        0 forwarded
        0 incoming packets discarded
        53935 incoming packets delivered
        26373 requests sent out
        34 fragments dropped after timeout
        107874 reassemblies required
        53920 packets reassembled ok
        34 packet reassemblies failed

On R2, we check fragments are created:

    Ip:
        Forwarding: 1
        11788288 total packets received
        11788275 forwarded
        0 incoming packets discarded
        4 incoming packets delivered
        11788288 requests sent out
        4468777 fragments received ok
        9 fragments failed
        8937471 fragments created

However, no stats are increased on R1.
