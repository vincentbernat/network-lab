node "cisco.local" {

  interface { "FastEthernet 0/1":
    description => "dummy interface",
    ipaddress => "192.168.14.1/24",
    ensure => present
  }

}
