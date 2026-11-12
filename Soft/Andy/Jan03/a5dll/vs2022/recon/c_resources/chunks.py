#!/usr/bin/env python
"""Dump the top-level chunk layout of a CStructureSaver file (game.db, resources, saves).

usage: python chunks.py <file> [max-chunks]

Layout as read by FileIO/BasicChunk1.cpp: <id:1 byte><length><payload>, where the length is one byte
(value >> 1) when its low bit is 0, or four bytes little-endian (value >> 1) when the low bit is 1.
"""
import sys

data = open(sys.argv[1], 'rb').read()
limit = int(sys.argv[2]) if len(sys.argv) > 2 else 20
pos, n = 0, 0
print('%s: %d bytes' % (sys.argv[1], len(data)))
while pos < len(data) and n < limit:
    cid = data[pos]
    b = data[pos + 1]
    if b & 1:
        length = int.from_bytes(data[pos + 1:pos + 5], 'little') >> 1
        hdr = 5
    else:
        length = b >> 1
        hdr = 2
    payload = data[pos + hdr:pos + hdr + length]
    print('  @%-10d id=%-3d len=%-10d %s' % (pos, cid, length, payload[:24].hex(' ')))
    pos += hdr + length
    n += 1
print('  end of walk at %d (%s)' % (pos, 'exact end of file' if pos == len(data) else 'NOT at end of file'))
