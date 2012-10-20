You can find more complete explanations
[on my blog](http://vincent.bernat.im/en/blog/2012-network-lab-kvm.html).

# Kernel configuration

This lab makes use of a special kernel. The configuration of this
kernel is provided in `config-3.6+ecmp`. The kernel used it is
`7fe0b14b725d6d09a1d9e1409bd465cb88b587f9` from net-next. You can use
anything from Linux 3.5.

The kernel configuration is pretty minimal and is targeted for this
lab. It should have the necessary drivers to use virtio (but nothing
else). There is no module. It also includes 9P support to be able to
mount a directory from guest.

You need the following patches:
 - [IPv6 ECMP support][1]
 - [overlayfs][2]

[1]: http://patchwork.ozlabs.org/patch/188562/
[2]: http://git.kernel.org/?p=linux/kernel/git/mszeredi/vfs.git;a=summary

To get a patch for overlayfs, you need a kernel tree from git with
both Linus tree and Miklos tree. Suppose you have cloned net-next and
you have applied the appropriate IPv6 ECMP patch in a dedicated branch
named `feature/ecmp-ipv6`.

     $ git remote add torvalds git://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux-2.6.git
     $ git fetch torvalds
     $ git remote add overlayfs git://git.kernel.org/pub/scm/linux/kernel/git/mszeredi/vfs.git
     $ git fetch overlayfs
     $ git merge-base overlayfs.v15 v3.6
     4cbe5a555fa58a79b6ecbb6c531b8bab0650778d
     $ git checkout -b feature/ecmp-ipv6+overlayfs
     $ git cherry-pick 4cbe5a555fa58a79b6ecbb6c531b8bab0650778d..overlayfs.v15

# Filesystem

We require a complete filesystem hosted into some directory. This is
not really important what distribution is used. It is possible to use
the root filesystem of the currently running system. A debootstrap is
OK. You can use something fancier like `pbuilder` or `schroot`.

# Quagga

Quagga is expected to be compiled with the following options:

     ../configure --enable-vtysh --localstatedir=/opt/quagga/run \
                  --prefix=/opt/quagga --sysconfdir=/opt/quagga/etc \
                  --enable-multipath=64 \
                  CFLAGS="-O0 -g"

# Run

To run the lab, run:

    $ ./setup

If you want to run the lab inside an alternative root file system, use:

    $ export ROOT=/var/cache/pbuilder/bases/debian.sid.lab.amd64
    $ sudo bind --mount -o ro /home ${ROOT}/home
    $ ./setup

