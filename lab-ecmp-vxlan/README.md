# ECMP and VXLAN/multicast

This is a small experiment to make VXLAN with multicast work over an
IP network with ECMP paths. Notably, ECMP paths start directly from
the host. We expect traffic to be able to use both interfaces.

With a naive setup, we have to specify the output interface.
