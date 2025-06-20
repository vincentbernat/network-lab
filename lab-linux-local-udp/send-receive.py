#!/usr/bin/env python3

import asyncio
import socket
import errno


async def udp_receiver(host="127.0.0.1", port=8888):
    """Coroutine to receive UDP packets"""
    loop = asyncio.get_event_loop()
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 10)
    sock.bind((host, port))
    sock.setblocking(False)  # Make socket non-blocking for asyncio

    print(f"UDP receiver listening on {host}:{port}")

    try:
        while True:
            _, addr = await loop.sock_recvfrom(sock, 1024)
            print("r", end='', flush=True)
            await asyncio.sleep(10)
    finally:
        sock.close()


async def udp_sender(target_host="localhost", target_port=8888):
    """Coroutine to send UDP packets periodically"""
    loop = asyncio.get_event_loop()
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setblocking(False)

    try:
        while True:
            await loop.sock_sendto(
                sock, b'.', (target_host, target_port)
            )
            print('.', end='', flush=True)
            await asyncio.sleep(1)
    finally:
        sock.close()


async def main():
    print("Starting UDP sender and receiver...")

    # Run both coroutines concurrently
    await asyncio.gather(udp_receiver(), udp_sender())


asyncio.run(main())
