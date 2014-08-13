node "cisco" {

  vlan { "99":
    description => "Sales department"
  }
  vlan { "100":
    description => "Marketing department"
  }

  interface { "FastEthernet 0/1":
      description => "dummy interface",
      mode => trunk,
      allowed_trunk_vlans => "99, 100"
  }

  interface { "Vlan99":
    description => "Sales department",
    ipaddress => "192.168.14.1/24"
  }
  interface { "Vlan100":
    description => "Marketing department",
    ipaddress => "192.168.15.1/24"
  }

}
