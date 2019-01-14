This is about this commit:

```
commit 6a9e461f6fe4434e6172304b69774daff9a3ac4c
Author: Mahesh Bandewar <maheshb@google.com>
Date:   Mon Sep 24 14:39:42 2018 -0700

    bonding: pass link-local packets to bonding master also.

    Commit b89f04c61efe ("bonding: deliver link-local packets with
    skb->dev set to link that packets arrived on") changed the behavior
    of how link-local-multicast packets are processed. The change in
    the behavior broke some legacy use cases where these packets are
    expected to arrive on bonding master device also.

    This patch passes the packet to the stack with the link it arrived
    on as well as passes to the bonding-master device to preserve the
    legacy use case.

    Fixes: b89f04c61efe ("bonding: deliver link-local packets with skb->dev set to link that packets arrived on")
    Reported-by: Michal Soltys <soltys@ziu.info>
    Signed-off-by: Mahesh Bandewar <maheshb@google.com>
    Signed-off-by: David S. Miller <davem@davemloft.net>
```

It is suspected to be useful for active/backup LAGs.
