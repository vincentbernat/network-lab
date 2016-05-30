# 6rd setup

This lab is about setting up 6rd access for a CE to provide IPv6
connectivity over an IPv4 network. Most routers (R1, R2, R3) are IPv4
only while only the border router (R4) is dualstack.

The 6rd gateways (BR1, BR2) are anycasted for redundancy purpose. BGP
sessions are using BFD to ensure a fast switchover.
