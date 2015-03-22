#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""Plug multiple IOU instances to various VDE switches.

This program will connect any number of IOU instances to various VDE
switches. It will read the ``NETMAP`` file provided to the various IOU
instances and create the pipeline to connect each IOU instance to a
VDE switch.

A ``NETMAP`` file is a file where each line represents an Ethernet bus
(a hub). Each element of the line is a triplet:

 - instance number
 - interface
 - hostname

Here is an example of ``NETMAP`` file::

    1:2/1@hostname    666:1/0@hostname
    2:0/1@hostname    666:2/0@hostname

Each IOU instance has its own instance number. This program uses a
unique instance ID but each port can be mapped to a different VDE
switch. In the example above, let's assume that we have two IOU
instances (1 and 2), one instance of this program (666) and two ports
(1/0 and 2/0).

On the command-line, each port major is mapped to a VDE switch socket.

Hostnames are ignored. Only local IOU are supported. The hostname can
be just omitted.

"""

from __future__ import print_function
from __future__ import unicode_literals

import sys
import os
import re
import logging
import logging.handlers
import argparse
import asyncio
import collections
import struct
import socket

from cffi import FFI

logger = logging.getLogger("iou2vde")
MTU = 1580


class Plug(object):
    """Generic plug."""

    def __init__(self, instance, major, minor):
        self._instance = instance
        self._major = major
        self._minor = minor

    def send(self, source, buffer):
        """Send data into the plug.

        :param source: other end of the wire
        :param buffer: data to be sent
        """
        raise NotImplementedError

    @property
    def instance(self):
        return self._instance

    @property
    def major(self):
        return self._major

    @property
    def minor(self):
        return self._minor

    def __hash__(self):
        return hash((self._instance, self._major, self._minor))

    def __eq__(self, other):
        if not isinstance(other, Plug):
            return NotImplemented
        return (self._instance, self._major, self._minor) == \
            (other._instance, other._major, other._minor)

    def __repr__(self):
        return "{}({}:{}/{})".format(self.__class__.__name__,
                                     self._instance,
                                     self._major,
                                     self._minor)


class VDEPlug(Plug):
    """Plug to a VDE switch."""

    _CDEF = """
#define LIBVDEPLUG_INTERFACE_VERSION ...

struct vdeconn;
typedef struct vdeconn VDECONN;
typedef int mode_t;
struct vde_open_args {
  int port;
  char *group;
  mode_t mode;
};

VDECONN *vde_open_real(char *, char *, int, struct vde_open_args *);
ssize_t vde_recv(VDECONN *, void *, size_t, int);
ssize_t vde_send(VDECONN *, const void *, size_t, int);
int vde_datafd(VDECONN *);
int vde_ctlfd(VDECONN *);
int vde_close(VDECONN *);
"""
    _ffi = FFI()
    _ffi.cdef(_CDEF)
    _C = _ffi.verify("#include <libvdeplug.h>", libraries=["vdeplug"])

    def __init__(self, instance, major, minor, sock):
        super().__init__(instance, major, minor)
        self._sock = sock
        self._conn = None
        self._loop = None
        self._others = None

    def _ctl_reader(self):
        """Read from VDE switch control."""
        logger.warning("Connection with {} has been lost".format(self))
        self.close()

    def _data_reader(self):
        """Read from VDE switch"""
        c = self._C
        ffi = self._ffi
        buffer = ffi.new("char[]", MTU)
        nx = c.vde_recv(self._conn, buffer, MTU, 0)
        if nx <= 0:
            logger.warning("Unable to receive from {}".format(self))
            self.close()
        logger.debug("{}: received {} bytes, send them to {}".format(
            self,
            nx,
            self._others))
        for other in self._others:
            other.send(self, ffi.buffer(buffer, nx))

    def close(self):
        c = self._C
        self._loop.remove_reader(c.vde_datafd(self._conn))
        self._loop.remove_reader(c.vde_ctlfd(self._conn))
        c.vde_close(self._conn)
        self._conn = None
        self._other = None

    def attach(self, loop, others):
        """Attach to VDE switch"""
        c = self._C
        ffi = self._ffi
        args = ffi.new("struct vde_open_args *")
        args.port = 0
        args.group = ffi.NULL
        args.mode = 0o700
        sock = ffi.new("char[]",
                       self._sock.encode("utf8"))
        description = ffi.new("char[]",
                              repr(self).encode("utf8"))
        self._conn = c.vde_open_real(sock,
                                     description,
                                     c.LIBVDEPLUG_INTERFACE_VERSION,
                                     args)
        if self._conn == ffi.NULL:
            self._conn = None
            raise RuntimeError("Unable to connect to VDE {}".format(self._sock))
        self._loop = loop
        self._others = others
        logger.debug("{} connected to VDE switch".format(self))
        loop.add_reader(c.vde_ctlfd(self._conn),
                        self._ctl_reader)
        loop.add_reader(c.vde_datafd(self._conn),
                        self._data_reader)

    def send(self, source, buffer):
        """Send data to the VDE switch"""
        if self._conn is None:
            logger.debug(
                "{}: cannot send data, connection to switch closed".format(
                    self))
            return
        c = self._C
        if c.vde_send(self._conn, buffer, len(buffer), 0) != len(buffer):
            logger.warning("{}: cannot send all data".format(self))

    def __repr__(self):
        return "VDEPlug({}:{}/{})".format(self._sock,
                                          self._major,
                                          self._minor)


def IOUPlugFactory(loop, pseudo_instance, netio):
    """Create a new set of IOPlug classes using the same pseudo instance."""
    if not os.path.isdir(netio):
        os.makedirs(netio)
    socket_path = os.path.join(netio, str(pseudo_instance))
    if os.path.exists(socket_path):
        os.unlink(socket_path)
    logger.debug("Attach to IOU netio socket {}".format(socket_path))
    sock = socket.socket(socket.AF_UNIX, socket.SOCK_DGRAM)
    sock.bind(socket_path)
    sock.setblocking(False)

    # Create a specialized class for this pseudo instance
    cls = type('IOUPlug', (IOUPlugBase,),
               dict(_vdeplugs={},
                    _iouplugs={},
                    _sock=sock,
                    _netio=netio))

    # Receive datagram callback
    loop.add_reader(sock, cls._reader)
    return cls


class IOUPlugBase(Plug):
    """Plug to a IOU instance."""

    @classmethod
    def _reader(cls):
        # We don't care about addr. Let's decode the header.
        fmt = '!HHBB2c'
        size = struct.calcsize(fmt)
        data, addr = cls._sock.recvfrom(MTU + size)
        header = data[:size]
        if len(header) != size:
            logger.warning("Incorrect size received from {}".format(addr))
            return
        dinstance, sinstance, diface, siface, m1, m2 = struct.unpack(fmt,
                                                                     header)
        if (m1, m2) != (b"\x01", b"\x00"):
            logger.warning(
                "Incorrect magic in header received from {}".format(addr))
            return
        smajor = siface & 0xf
        sminor = (siface >> 4) & 0xf
        dmajor = diface & 0xf
        dminor = (diface >> 4) & 0xf
        destination = Plug(dinstance, dmajor, dminor)
        destination = cls._vdeplugs.get(destination, None)
        source = Plug(sinstance, smajor, sminor)
        source = cls._iouplugs.get(source, None)
        if destination is None:
            logger.warning(
                "Received incorrect destination from {} ({}:{}/{})".format(
                    addr, dinstance, dmajor, dminor))
            return
        if source is None:
            logger.warning(
                "Received incorrect source from {} ({}:{}/{})".format(
                    addr, sinstance, smajor, sminor))
        destination.send(source, data[size:])

    def attach(self, loop, other):
        # Register to our per-class registry
        self._vdeplugs[other] = other
        self._iouplugs[self] = self

    def send(self, source, buffer):
        """Send data to the IOU instance."""
        # IOU header format
        # Pos (byte)    value
        # ==============================================================
        # 00 - 01       destination (receiving) IOU instance ID
        # 02 - 03       source (sending) IOU instance ID
        # 04            receiving interface ID
        # 05            sending interface ID
        # 06 - 07       fixed delimiter, looks like its always 0x01 0x00
        header = struct.pack("!HHBB2c",
                             self._instance,
                             source.instance,
                             self._minor << 4 | self._major,
                             source.minor << 4 | source.major,
                             b"\x01", b"\x00")
        try:
            self._sock.sendto(header + buffer,
                              os.path.join(self._netio, str(self._instance)))
        except FileNotFoundError:
            logger.warning("unable to send to {}: no appropriate socket".format(self))


def parse():
    """Parse arguments"""
    parser = argparse.ArgumentParser(
        description=sys.modules[__name__].__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)

    g = parser.add_mutually_exclusive_group()
    g.add_argument("--debug", "-d", action="store_true",
                   default=False,
                   help="enable debugging")
    g.add_argument("--silent", "-s", action="store_true",
                   default=False,
                   help="don't log to console")

    parser.add_argument("--netio", metavar="DIR",
                        default="/tmp/netio{}".format(os.getuid()),
                        help="Directory for netio sockets")
    parser.add_argument("--netmap", "-N", metavar="NETMAP", type=open,
                        default="NETMAP",
                        help="NETMAP file")
    parser.add_argument("--instance", "-p", metavar="INSTANCE", type=int,
                        default=666,
                        help="Pseudo instance number for this program")
    parser.add_argument("mapping", nargs='+', metavar="PORT:VDESOCKET",
                        help="Port to VDE socket mapping")

    options = parser.parse_args()
    return options


def parse_netmap(instance, netmap, mappings, IOUPlug):
    """Parse a NETMAP file to get a mapping from VDE socket to IOU instances"""
    iire = re.compile(r"^(?P<instance>\d+):(?P<major>\d+)/(?P<minor>\d+)(?:@(?P<hostname>[\w-]+))?")
    wires = collections.defaultdict(list)
    for mapping in mappings:
        major, sock = mapping.split(":")
        major = int(major)
        netmap.seek(0)
        for line in netmap:
            iou_instances = [iire.match(ii) for ii in re.split(r"\s+", line)]
            iou_instances = [ii for ii in iou_instances if ii]
            us = [ii
                  for ii in iou_instances
                  if int(ii.group("instance")) == instance
                  and int(ii.group("major")) == major]
            if not us:
                continue
            minor = int(us[0].group("minor"))
            iou_instances = [ii
                             for ii in iou_instances
                             if ii not in us]
            vdeplug = VDEPlug(instance, major, minor, sock)
            iouplugs = [IOUPlug(int(ii.group("instance")),
                                int(ii.group("major")),
                                int(ii.group("minor")))
                        for ii in iou_instances]
            wires[vdeplug].extend(iouplugs)
    for vdeplug in wires:
        for iouplug in wires[vdeplug]:
            logger.info("{} ↔ {}".format(vdeplug, iouplug))
    return wires


def setup_logging(debug, silent):
    """Setup logger"""
    logger.setLevel(debug and logging.DEBUG or logging.INFO)
    if sys.stderr.isatty() and not silent:
        ch = logging.StreamHandler()
        ch.setFormatter(logging.Formatter(
            "%(levelname)s[%(name)s] %(message)s"))
        logger.addHandler(ch)


def setup_vde(loop, wires):
    """Setup connection to VDE switches.

    :param loop: asyncio's event loop
    :param wires: mapping from VDEPlug instances to IOUPlug instances
    """
    for vdeplug in wires:
        vdeplug.attach(loop, wires[vdeplug])
        for iouplug in wires[vdeplug]:
            iouplug.attach(loop, vdeplug)


def stop_loop(loop, context):
    logger.exception("Uncatched exception: %s", context['exception'])
    loop.stop()

if __name__ == "__main__":
    options = parse()
    setup_logging(options.debug, options.silent)
    try:
        os.umask(0o077)
        loop = asyncio.get_event_loop()
        loop.set_exception_handler(stop_loop)
        IOUPlug = IOUPlugFactory(loop, options.instance, options.netio)
        wires = parse_netmap(options.instance,
                             options.netmap,
                             options.mapping,
                             IOUPlug)
        setup_vde(loop, wires)
        try:
            logger.debug("Start main loop")
            loop.run_forever()
        except KeyboardInterrupt:
            pass
        finally:
            loop.close()
    except Exception as e:
        logger.exception("Uncatched exception: %s", e)
