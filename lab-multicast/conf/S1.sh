#!/bin/sh

ip addr add 10.234.95.2/24 dev eth0
ip route add default via 10.234.95.1

exec /bin/bash
