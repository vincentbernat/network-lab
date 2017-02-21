# ECMP and VXLAN/unicast

This simple lab setup VXLAN using unicast. For each VXLAN, default
entries are programmed in the FDB to other hosts. VXLAN then does a
good job to learn entries automatically. Use `bridge fdb show dev vx4`
to see learn entries.

# ECMP and VXLAN/multicast

See commit 2a2d5a7c940c for tentatives to use multicast. The
conclusion is that I am either too dumb to make it work or advanced
multicast routing (using loopback interfaces) is too limited with
Linux.
