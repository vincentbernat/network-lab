#!/usr/bin/python

from scapy.all import *

# Inject 1000 packets
send([IPv6(src=RandIP6("2001:db8:beef:**"),
           dst=RandIP6("2001:db8:beef:**"))/ICMPv6EchoRequest()/"12345678"]*1000, inter=0.01)
