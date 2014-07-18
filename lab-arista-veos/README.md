Lab with Arista vEOS
====================

Arista vEOS is a virtual version of Arista EOS.

Lab
---

This lab is an experiment with VARP. VARP is meant to replace first
hop router redundancy protocols like VRRP and HSRP. Unfortunately,
those protocols are usually working in active/standby mode. VARP is an
attempt to correct that by assigning each virtual router a MAC address
shared by all the devices of the virtual router.

Download
--------

You can download vEOS from [Arista website][]. You need both
`Aboot-veos-serial-x.x.x.iso` and `vEOS-x.xx.xx.vmdk`. Convert the
last one with `qemu-img` to the qcow2 format:

    qemu-img convert -O qcow2 vEOS-x.xx.xx.{vmdk,img}

Symlink them in `images/` as `Aboot-veos.iso` and `vEOS.img`. You can
set them read-only for safety. We use COW to avoid any modification of
the original image.

It seems there is a bug in `Aboot-veos-serial-2.0.8.iso` which makes
it not serial-ready at all. A small shell script,
`images/fix-arista-veos-iso-serial.sh` will fix the ISO. It needs
`guestfish` (available in `libguestfs-tools` package).

Also, the image is shipped with an empty `startup-config`. This
prevent ZTP to work. This can be fixed with the shell script
`images/fix-arista-veos-image.sh`. It also needs guestfish.

[Arista website]: https://www.arista.com/en/support/software-download
