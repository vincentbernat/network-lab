Lab with Cisco IOU
==================

Cisco IOU (or Cisco IOL) is a version of IOS running on Linux. The
advantage over using things like Dynamips/Dynagen is the better CPU
usage. You can find more information about it on
[Jeremy Gaddis' FAQ](http://evilrouters.net/2011/01/18/cisco-iou-faq/).

Lab
---

This lab is quite simple. Two IOU instances and two Linux running BIRD
are plugged on the same virtual switch and establish OSPF adjacencies
between them (with BFD for faster convergence times).

This lab is incomplete and doesn't work. I have yet to figure out how
to make IOU speaks with the VDE swicth correctly. VDE seems to prefix
the Ethernet frame with a port number. Not what I want.

Images
------

There is some duplicity when people talks about IOU. Only a few
persons at Cisco are authorized to use it. However, if you search for
`Cisco IOU Collection`, you should find something suitable on
Bittorrent.

Once you have obtained your IOU images, link your favorite pick to
`images/i86bi-linux-l3.bin`.

You may need to patch it if you get an error like this:

    $ ../images/i86bi-linux-l3.bin --help
    ../images/i86bi-linux-l3.bin: error while loading shared libraries: libcrypto.so.4: cannot open shared object file: No such file or directory
    $ sudo ln -s libcrypto.so.1.0.0 /usr/lib/i386-linux-gnu/libcrypto.so.4

You also need to update your `~/.iourc` file. You may have downloaded
a `keygen.py` script just for that.
