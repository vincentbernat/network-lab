# -*- sh -*-

UTS=$(uname -n)

[ -d /var/run/quagga ] || mkdir -p /var/run/quagga
[ -d /var/log/quagga ] || mkdir -p /var/log/quagga
chown quagga:quagga /var/run/quagga
chown quagga:quagga /var/log/quagga

LIBQUAGGA=/usr/lib/quagga

for bin in zebra ospfd bgpd; do
    [ ! -f $PWD/conf/${UTS}-${bin}.conf ] || {
	echo -n "Starting $bin... "
	$LIBQUAGGA/${bin} -d -f $PWD/conf/${UTS}-${bin}.conf -A 127.0.0.1
	echo "Done!"
    }
done
export VTYSH_PAGER=/bin/cat
