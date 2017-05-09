# Experiments with Linux IPv4 FIB trie

After inserting routes, see:

 - `/proc/net/fib_trie`
 - `/proc/net/fib_triestat`

To get a MRT dump:

    wget http://data.ris.ripe.net/rrc00/latest-bview.gz

## Results

Here are results from `/proc/net/fib_triestat`. Note that `main` and
`local` tables are merged, so they have the same stats. We only show
`main`.

### 100k /32

    $ cat /proc/net/fib_triestat
    Basic info: size of leaf: 48 bytes, size of tnode: 40 bytes.
    Main:
            Aver depth:     4.99
            Max depth:      5
            Leaves:         100010
            Prefixes:       100017
            Internal nodes: 10381
              1: 1  2: 1  3: 3  4: 10100  5: 100  6: 145  7: 29  8: 2
            Pointers: 178334
    Null ptrs: 67944
    Total size: 11957  kB

### 100k partial view

    $ cat /proc/net/fib_triestat
    Basic info: size of leaf: 48 bytes, size of tnode: 40 bytes.
    Main:
            Aver depth:     2.79
            Max depth:      6
            Leaves:         98704
            Prefixes:       100017
            Internal nodes: 38216
              1: 4  2: 29081  3: 6459  4: 1867  5: 686  6: 116  7: 2  15: 1
            Pointers: 260276
    Null ptrs: 123357
    Total size: 13623  kB
