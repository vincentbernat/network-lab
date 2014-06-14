Lab with Juniper Firefly Perimeter
==================================

Firefly Perimeter is a virtual SRX appliance. You can download it from
[Juniper website][]. Choose the KVM image and extract it by using the
`-x` flag. This should get you three files. One of them is the
image. Put i in the `images/` directory and name it `junos-vsrx.img`.

[Juniper website]: http://www.juniper.net/us/en/products-services/security/firefly-perimeter/#evaluation

When booting, the Juniper are starting from the factory default
configuration and we exploit the auto installation feature to download
the correct configuration through TFTP. To troubleshoot, look at what
is happening in `/var/log/autod` on each Juniper.

This lab is quite simple. Two Juniper SRX and two Linux running BIRD
are plugged on the same virtual switches and we establish OSPF
adjacencies between them (with BFD for faster convergence times).
