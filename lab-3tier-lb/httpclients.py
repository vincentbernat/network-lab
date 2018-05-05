#!/usr/bin/env python3

"""Simulate many clients toward a given set of IP services. Provide a
small graph on screen to show when errors occurs. IPs of the clients
should be on the loopback interface. The service IPs are provided on
the command-line.

For example::

    ./httpclients.py \
        198.51.100.1:80 \
        198.51.100.2:80 \
        [2001:db8::198.51.100.1]:80 \
        [2001:db8::198.51.100.2]:80
"""

import aiohttp
import asyncio
import json
import random
import subprocess
import sys


async def fetch(source, destination):
    """HTTP request to the provided destination (IP, port) using the
    provided source (IP, port)."""
    host, port = destination
    url = f'http://[{host}]:{port}/10M'
    conn = aiohttp.TCPConnector(local_addr=(source, 0))
    async with aiohttp.ClientSession(connector=conn) as client:
        async with client.get(url) as resp:
            while True:
                await asyncio.sleep(random.random()*5)
                got = await asyncio.wait_for(resp.content.readany(), timeout=2)
                if not got or random.random() > 0.95:
                    break
                del got


async def main(sources, destinations):
    parallel = 50
    pending = set()
    failures = 0
    successes = 0
    while True:
        source = random.choice(sources)
        destination = random.choice(destinations)
        if ':' in source and ':' not in destination[0]:
            continue
        if ':' in destination[0] and ':' not in source:
            continue
        pending.add(fetch(source, destination))
        if len(pending) >= parallel:
            done, pending = await asyncio.wait(
                pending,
                return_when=asyncio.FIRST_COMPLETED)
            for d in done:
                try:
                    await d
                except:
                    failures += 1
                else:
                    successes += 1
                if failures + successes == 8:
                    if successes == 0:
                        c = "\033[1;31m_\033[0;0m"
                    elif successes == 8:
                        c = "\033[1;32m⣿\033[0;0m"
                    else:
                        c = "\033[1;31m{}\033[0;0m".format(
                            chr(0x2800 + (1 << 8) - (1 << failures)))
                    print(c, end='', flush=True)
                    failures = 0
                    successes = 0


def parse_destination(destination):
    ip, port = destination.rsplit(":", 1)
    if ip[0] == '[' and ip[-1] == ']':
        ip = ip[1:-1]
    return (ip, int(port))


def get_sources():
    s = subprocess.run(["ip", "-json", "addr", "list", "dev", "lo"],
                       stdout=subprocess.PIPE,
                       check=True)
    info = json.loads(s.stdout.decode('utf-8'))
    return [addr['local']
            for addr in info[0]['addr_info']
            if addr['scope'] == 'global']


destinations = [parse_destination(arg) for arg in sys.argv[1:]]
sources = get_sources()
loop = asyncio.get_event_loop()
try:
    loop.run_until_complete(main(sources, destinations))
except KeyboardInterrupt:
    pass
