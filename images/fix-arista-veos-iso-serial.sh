#!/bin/sh

# This script is not needed anymore for Aboot-veos-serial-2.1.0.iso

set -e

if [ $# -ne 1 ]; then
    cat <<EOF 1>&2
Usage: $0 Aboot-veos-serial.x.x.x.iso

Fix Aboot-veos-serial-2.0.8.iso image to really be
"serial-ready".
EOF
    exit 1
fi
iso="$(readlink -f "$1")"

tmp=$(mktemp -d)
trap "rm -rf $tmp" EXIT
cd $tmp

echo '[+] Extracting ISO'
guestfish -a "$iso" -m /dev/sda tar-out / - | tar xf -
chmod u+w * .
if grep -q '^append .* console=ttyS0' isolinux.cfg; then
    echo '[!] ISO already fixed'
    exit 0
fi

echo '[+] Patching isolinux.cfg'
sed -i 's/^\(append .*\)/\1 console=ttyS0/' isolinux.cfg

echo '[+] Rebuilding ISO'
mv -n "$iso" "$iso.backup"
genisoimage -quiet -o "$iso" -b isolinux.bin -c boot.cat \
    -no-emul-boot -boot-load-size 4 -boot-info-table $PWD

echo '[!] Done'
