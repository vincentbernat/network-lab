# Salt with Juniper

This is a minimal lab using Salt and `salt-proxy` to manage a bunch of
Juniper devices. Once running, the keys for the 3 `salt-proxy`
processes may have to be accepted on the master:

```console
$ salt-key -A
The following keys are going to be accepted:
Unaccepted Keys:
juniper1
juniper2
juniper3
Proceed? [n/Y] y
Key for minion juniper1 accepted.
Key for minion juniper2 accepted.
Key for minion juniper3 accepted.
```

Then, you can query the various proxies:

```console
$ salt '*' test.version
juniper3:
    2019.2.2
juniper2:
    2019.2.2
juniper1:
    2019.2.2
```

Check if they are connected to the Juniper devices:

```console
$ salt '*' net.connected
juniper3:
    ----------
    out:
        True
juniper2:
    ----------
    out:
        True
juniper1:
    ----------
    out:
        True
```

Grab ARP tables:

```console
$ salt --out=yaml juniper1 net.arp
juniper1:
  comment: ''
  out:
  - age: 648.0
    interface: em0.0
    ip: 10.0.2.2
    mac: 52:55:0A:00:02:02
  result: true
```

To get serial numbers:

```console
$ salt --out=yaml '*' grains.get serial
juniper2: VR5DC2E09B40
juniper3: VR5DC2E09CD3
juniper1: VR5DC2E09CD3
```

To get versions:

```console
$ salt --out=yaml \* net.cli 'show version | match Junos:'
juniper3:
  comment: ''
  out:
    'show version | match Junos:': 'Junos: 17.3R3.10'
  result: true
juniper2:
  comment: ''
  out:
    'show version | match Junos:': 'Junos: 17.3R3.10'
  result: true
juniper1:
  comment: ''
  out:
    'show version | match Junos:': 'Junos: 17.3R3.10'
  result: true
```
