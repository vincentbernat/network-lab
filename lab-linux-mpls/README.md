# MPLS lab for Linux

 - https://blog.swineson.me/en/use-linux-as-an-mpls-router/

This is mostly to implement error handling in MPLS stack. This works
with OpenBSD:

```
 20:36 ❱ ./mtr -wzbe 192.0.2.36
Start: 2021-04-02T20:36:05+0200
HOST: C1                    Loss%   Snt   Last   Avg  Best  Wrst StDev
  1. AS???    192.0.2.1      0.0%    10    0.7   0.7   0.5   0.9   0.1
  2. AS???    203.0.113.1    0.0%    10    1.2   1.2   0.7   1.6   0.3
  3. AS???    169.254.0.3    0.0%    10    2.6   2.7   1.1   3.6   0.7
       [MPLS: Lbl 21 TC 0 S u TTL 1]
       [MPLS: Lbl 80 TC 0 S u TTL 1]
  4. AS???    ???           100.0    10    0.0   0.0   0.0   0.0   0.0
  5. AS???    203.0.113.36   0.0%    10    1.5   2.5   1.3   3.9   1.0
  6. AS???    192.0.2.36     0.0%    10    1.4   3.8   1.4   5.2   1.0
```

Hop 3 is OpenBSD doing the right thing and hop 4 is Linux, not doing
the right thing.
