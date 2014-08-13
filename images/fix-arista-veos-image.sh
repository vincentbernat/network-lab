#!/bin/sh


set -e

if [ $# -ne 1 ]; then
    cat <<EOF 1>&2
Usage: $0 vEOS-x.x.x.img

Fix vEOS-4.13.7M.img image to remove startup-config file and
enable ZTP.
EOF
    exit 1
fi
img="$(readlink -f "$1")"

tmp=$(mktemp -d)
trap "rm -rf $tmp" EXIT
cd $tmp

echo '[+] Removing startup-config'
chmod u+w "$img"
guestfish -a "$img" -m /dev/sda1 rm /startup-config
chmod u-w "$img"
echo '[!] Done'
