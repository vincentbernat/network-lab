#!/usr/bin/env python3

import sys
import socket

s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
s.setsockopt(socket.SOL_SOCKET, socket.SO_BINDTODEVICE, b"public")
s.connect((sys.argv[1], 80))
print("connected {} → {}".format(
    s.getsockname(),
    s.getpeername()))
