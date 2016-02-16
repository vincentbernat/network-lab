Lab with Juniper vMX and full views
===================================

Each vMX will receive a eBGP fullview. This needs
[gobgp](https://github.com/osrg/gobgp) and an MRT dumpfile. To get
one, use:

    wget http://data.ris.ripe.net/rrc00/latest-bview.gz

So, you need the following symlinks for this lab to work:

 - `gobgp`
 - `gobgpd`
 - `latest-bview.gz`

Due to the memory needed for a full view, this lab is not really using
a full view. Have a look at `./setup` and search for `start_gobgp` to
see what part of the full view is used.
