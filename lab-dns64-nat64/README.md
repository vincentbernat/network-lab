# NAT64/DNS64 setup

This is a very simple setup where `gateway` acts as a DNS64/NAT64
gateway (as well as a regular IPv6 gateway). `client` is v6-only and
can access a v4-only host:

    $ curl -i v4.example.com
    HTTP/1.1 200 OK
    Server: nginx/1.14.2
    Date: Mon, 08 Jul 2019 21:11:06 GMT
    Content-Type: text/html
    Content-Length: 4
    Last-Modified: Mon, 08 Jul 2019 21:03:25 GMT
    Connection: keep-alive
    ETag: "5d23af9d-4"
    X-Remote-Addr: ::ffff:192.0.2.15
    X-Remote-Port: 45156
    X-Server-Addr: ::ffff:203.0.113.16
    X-Server-Port: 80
    Accept-Ranges: bytes
    
    www

This works because unbound will act as a DNS64 server and add a AAAA
record if none exists:

    $ host v4.example.com
    v4.example.com has address 203.0.113.16
    v4.example.com has IPv6 address 2001:db8:ff9b::cb00:7110

A dual-stack host is queried using IPv6.

    $ curl -i www.example.com
    HTTP/1.1 200 OK
    Server: nginx/1.14.2
    Date: Mon, 08 Jul 2019 21:11:00 GMT
    Content-Type: text/html
    Content-Length: 4
    Last-Modified: Mon, 08 Jul 2019 21:03:25 GMT
    Connection: keep-alive
    ETag: "5d23af9d-4"
    X-Remote-Addr: 2001:db8:cccc:0:5254:33ff:fe00:1
    X-Remote-Port: 45760
    X-Server-Addr: 2001:db8:dead::16
    X-Server-Port: 80
    Accept-Ranges: bytes
    
    www

It's using Tayga for NAT64. This is not something remotely ready to be
used in production.
