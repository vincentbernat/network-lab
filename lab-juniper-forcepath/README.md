# Force a destination to a given path

In presence of multiple possible paths, this is a tentative to force a
destination through one path and the other through the other path. The
trick is to just use a static route with a next-hop that should be
resolved to the directly attached transit provider. This assumes AS
paths and local preference between the transit providers are the same.

However, this doesn't work as if a transit provider goes down, the
attached router will still advertise a most specific route which will
be picked by the neighbor router:

```
[edit]
juniper@router# run show route table router1.inet.0 198.51.100.34/32

router1.inet.0: 10 destinations, 11 routes (10 active, 0 holddown, 0 hidden)
+ = Active Route, - = Last Active, * = Both

198.51.100.34/32   *[BGP/140] 00:04:46, localpref 100
                      AS path: I, validation-state: unverified
                    > to 192.0.2.1 via lt-0/0/0.1

[edit]
juniper@router# run show route table router2.inet.0 198.51.100.34/32

router2.inet.0: 10 destinations, 11 routes (10 active, 0 holddown, 0 hidden)
+ = Active Route, - = Last Active, * = Both

198.51.100.34/32   *[Static/5] 00:04:50, metric2 0
                    > to 192.0.2.0 via lt-0/0/0.2
```
