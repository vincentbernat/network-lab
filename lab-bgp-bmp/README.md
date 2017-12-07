# Quick tests with BGP BMP

Receiver is gobgmpd (from GoBGP). Sender is a Juniper vRR.

## Notes

 - On 15.1, despite BMP is disabled on a peer, an header will still be
   sent as well as a BGP notification:

        {"Header":{"Version":3,"Length":176,"Type":3},"PeerHeader":{"PeerType":0,"Flags":128,"PeerDistinguisher":0,"PeerAddress":"2001:db8:1::2","PeerAS":65000,"PeerBGPID":"1.0.0.1","Timestamp":1512640571},"Body":{"LocalAddress":"2001:db8:1::fffe","LocalPort":51108,"RemotePort":179,"SentOpenMsg":{"Header":{"Marker":null,"Len":63,"Type":1},"Body":{"Version":4,"MyAS":65000,"HoldTime":90,"ID":"1.0.0.0","OptParamLen":34,"OptParams":[{"ParamType":2,"ParamLen":6,"Capability":[{"code":1,"value":131073}]},{"ParamType":2,"ParamLen":2,"Capability":[{"code":128}]},{"ParamType":2,"ParamLen":2,"Capability":[{"code":2}]},{"ParamType":2,"ParamLen":4,"Capability":[{"code":64,"flags":4,"time":120,"tuples":null}]},{"ParamType":2,"ParamLen":6,"Capability":[{"code":65,"value":65000}]},{"ParamType":2,"ParamLen":2,"Capability":[{"code":71}]}]}},"ReceivedOpenMsg":{"Header":{"Marker":null,"Len":45,"Type":1},"Body":{"Version":4,"MyAS":65000,"HoldTime":90,"ID":"1.0.0.1","OptParamLen":16,"OptParams":[{"ParamType":2,"ParamLen":14,"Capability":[{"code":2},{"code":1,"value":131073},{"code":65,"value":65000}]}]}}}}
        {"Header":{"Version":3,"Length":51,"Type":2},"PeerHeader":{"PeerType":0,"Flags":128,"PeerDistinguisher":0,"PeerAddress":"2001:db8:1::2","PeerAS":65000,"PeerBGPID":"1.0.0.1","Timestamp":1512640571},"Body":{"Reason":2,"BGPNotification":null,"Data":"AAA="}}
