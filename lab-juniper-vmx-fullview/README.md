Lab with Juniper vMX and full views
===================================

Each vMX will receive a eBGP fullview. This needs ExaBGP, mrtparse and
an MRT dumpfile.

There is also a tentative to not install all routes in FIB. vMX1 and
vMX2 are not using the same method to do so: vMX1 tries to use the
state of the default route (but this doesn't work). vMX2 only
discriminates on specific route attributes. You can look at the FIB
with `show route forwarding-table`.

There is a limit for the vMX in lite mode to the number of routes
accepted in RIB. That's why we only accept 10k routes.

MRT dump
--------

To get one, use:

    wget http://data.ris.ripe.net/rrc00/latest-bview.gz
