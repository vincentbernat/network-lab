#!/usr/bin/env python3

"""Execute a bunch of HTTP requests using provided source/destinations
and log the result.

Should be run with a list of source/destination tuples as
arguments. For example::

    ./httprequests.py \
        203.0.113.105:38447,203.0.113.15:80 \
        203.0.113.106:38457,203.0.113.15:80 \
        203.0.113.106:31447,203.0.113.15:80 \
        203.0.113.106:28447,203.0.113.15:80 \
        203.0.113.105:38487,203.0.113.15:80 \
        203.0.113.105:38147,203.0.113.15:80 \
        203.0.113.105:31210,203.0.113.15:80

Could be used with ``xargs``::

    for i in $(seq 120 125); do
      for c in $(seq 1 2000); do
        echo 203.0.113.$i:$((RANDOM%10000 + 30000)),203.0.113.15:80
      done
    done \
    | sort | uniq \
    | xargs -n1000 ./httprequests.py \
    | awk '{print $NF}' \
    | sort | uniq -c
"""

import aiohttp
import asyncio
import sys


async def fetch(source, destination):
    """HTTP request to the provided destination (IP, port) using the
    provided source (IP, port)."""
    host, port = destination
    url = f'http://{host}:{port}/'
    conn = aiohttp.TCPConnector(local_addr=source)
    async with aiohttp.ClientSession(connector=conn) as client:
        start = loop.time()
        async with client.get(url) as resp:
            status = resp.status
            got = await resp.text()
            got = got.splitlines()[0]
            end = loop.time()
            elapsed = (end - start) * 1000
            print(f'{source[0]}:{source[1]} → {host}:{port} : '
                  f'{int(elapsed)}ms {status} {got}')


async def main(pairs):
    await asyncio.wait([fetch(pair[0], pair[1])
                        for pair in pairs])


def parse_pair(pair):
    source, destination = pair.split(",")
    ip, port = source.split(":")
    source = (ip, int(port))
    ip, port = destination.split(":")
    destination = (ip, int(port))
    return (source, destination)


pairs = (parse_pair(arg) for arg in sys.argv[1:])
loop = asyncio.get_event_loop()
loop.run_until_complete(main(pairs))
