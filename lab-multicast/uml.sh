#!/bin/sh

# UML "init"
hostname -b ${uts}
export TERM=xterm
export PATH=/usr/local/bin:/usr/bin:/bin:/sbin:/usr/local/sbin:/usr/sbin

# A getty allow to setup a proper console
[ x"$GETTY" = x"1" ] || {
    export GETTY=1
    getty -n -l $0 38400 /dev/ttyS0
}

# FS
mount -t proc proc /proc
mount -t sysfs sysfs /sys
mount -t tmpfs tmpfs /dev -o rw && {
    cd /dev
    MAKEDEV null
}
mount -t tmpfs tmpfs /var/run -o rw,nosuid,nodev
mount -t tmpfs tmpfs /var/tmp -o rw,nosuid,nodev
mount -t tmpfs tmpfs /var/log -o rw,nosuid,nodev
mount -t tmpfs tmpfs /etc/racoon
mount -o bind /usr/lib/uml/modules /lib/modules
mount -t hostfs hostfs $(dirname $0) -o $(dirname $0)

# Interfaces
for intf in /sys/class/net/*; do
    intf=$(basename $intf)
    ip a l dev $intf 2> /dev/null >/dev/null && ip link set up dev $intf
done

# Syslog
rsyslogd

cd $(dirname $0)
exec $PWD/conf/${uts}.sh
