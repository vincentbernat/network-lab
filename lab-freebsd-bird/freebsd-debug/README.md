# Debugging FreeBSD kernel

Export gdb UNIX socket to `freebsd-debug`:

    ssh -R /tmp/gdb.pipe:/tmp/tmp.knvdSM8IGy/vm-freebsd-gdb.pipe -F /tmp/tmp*/ssh_config(om[1]) freebsd-debug.lab

Then, as usual:

    $ gdb /usr/lib/debug/boot/kernel/kernel.debug
    Reading symbols from /usr/lib/debug/boot/kernel/kernel.debug...
    (gdb) target remote | socat STDIO UNIX:/tmp/gdb.pipe
    Remote debugging using | socat STDIO UNIX:/tmp/gdb.pipe
