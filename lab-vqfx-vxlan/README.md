# Multihomed VXLAN BGP EVPN with Juniper vQFX

This is a tentative, but unfortunately, Cumulus FRR is discarding BGP
updates when they contain an ESI tag. Therefore, this doesn't
work. Also, Juniper says the VRF target of received BGP updates are
invalid. This should be solvable, but didn't investigate more.
