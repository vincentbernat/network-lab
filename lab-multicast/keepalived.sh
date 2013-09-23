#!/bin/sh

UTS=$(uname -n)
KEEPALIVED=keepalived

[ -f conf/${UTS}-keepalived.conf ] && {
    echo -n "Starting keepalived... "
    $KEEPALIVED -f $PWD/conf/${UTS}-keepalived.conf
    echo "Done!"
}
