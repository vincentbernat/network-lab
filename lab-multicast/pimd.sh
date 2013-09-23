#!/bin/sh

UTS=$(uname -n)

[ -f conf/${UTS}-pimd.conf ] && {
    echo -n "Starting pimd... "
    pimd -c $PWD/conf/${UTS}-pimd.conf -N
    echo "Done!"
}
