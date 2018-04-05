# Multihomed VXLAN BGP EVPN with Juniper vQFX

This is a tentative, but unfortunately, Cumulus FRR is discarding BGP
updates when they contain an ESI tag. Therefore, this doesn't
work. Also, Juniper says the VRF target of received BGP updates are
invalid. FRR needs to be patched for this... See the vxlan lab which
includes an option to use a vQFX instead.
