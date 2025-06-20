# UDP local delivery on Linux

With Linux, IP local delivery is done synchronously: loopback xmit method (in
`drivers/net/loopback.c`) is calling `__netif_rx()` to deliver the packet. I
mistakently believed that this would allow reliable delivery with UDP on
loopback, providing a reliable datagram delivery option.

```
# bpftrace  -e 'kprobe:udp_queue_rcv_skb { print(kstack); }'

        udp_queue_rcv_skb+1
        udp_unicast_rcv_skb+118
        __udp4_lib_rcv+2721
        ip_protocol_deliver_rcu+213
        ip_local_deliver_finish+118
        ip_local_deliver+103
        __netif_receive_skb_one_core+133
        process_backlog+135
        __napi_poll+43
        net_rx_action+820
        handle_softirqs+223
        do_softirq.part.0+59
        __local_bh_enable_ip+96
        __dev_queue_xmit+620
        ip_finish_output2+738
        ip_send_skb+137
        udp_send_skb+398
        udp_sendmsg+2471
        __sys_sendto+467
        __x64_sys_sendto+36
        do_syscall_64+130
        entry_SYSCALL_64_after_hwframe+118
```

This small example shows this is false. One coroutine is reading packets, the
other is sending them a bit faster. We can check with `ss` that we are dropping
packets:

```
# ss -aupen --info --extended --memory sport = :8888
State       Recv-Q      Send-Q           Local Address:Port            Peer Address:Port      Process
UNCONN      2880        0                    127.0.0.1:8888                 0.0.0.0:*          users:(("python3",pid=763,fd=6))       ino:3559 sk:b cgroup:unreachable:1 <->
         skmem:(r2880,rb2304,t0,tb212992,f1216,w0,o0,bl0,d6)
```

BTW, I didn't find a way to use `MSG_ERRQUEUE` with asyncio, so I can't detect
the delivery failed. I think this should be possible.
