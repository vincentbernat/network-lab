#!/bin/sh

ip addr add 10.234.72.196/29 dev eth0
ip route add default via 10.234.72.193

exec /bin/bash
