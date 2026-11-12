"""Heuristic recursive dump of a CStructureSaver file: python -I dump.py <file> [maxdepth] [maxlines]
A payload is shown as nested chunks if it parses exactly as a chunk sequence (Jan03 BasicChunk1.cpp rules)."""
import sys

def parse(buf):
    """Return list of (id, payload) if buf parses exactly into chunks, else None."""
    pos, out = 0, []
    while pos < len(buf):
        if pos + 2 > len(buf):
            return None
        cid = buf[pos]
        b = buf[pos + 1]
        if b & 1:
            if pos + 5 > len(buf):
                return None
            ln = int.from_bytes(buf[pos + 1:pos + 5], 'little') >> 1
            h = 5
        else:
            ln = b >> 1
            h = 2
        if pos + h + ln > len(buf):
            return None
        out.append((cid, buf[pos + h:pos + h + ln]))
        pos += h + ln
    return out

lines = 0
def show(chunks, depth, maxdepth, maxlines, path):
    global lines
    prev = None; rep = 0
    for i, (cid, p) in enumerate(chunks):
        sub = parse(p) if (depth < maxdepth and len(p) >= 2) else None
        key = (cid, len(p), sub is not None)
        if lines >= maxlines:
            return
        print('%s%d len=%d %s' % ('  ' * depth, cid, len(p), '' if sub else p[:16].hex(' ')))
        lines += 1
        if sub:
            show(sub, depth + 1, maxdepth, maxlines, path + [cid])

data = open(sys.argv[1], 'rb').read()
md = int(sys.argv[2]) if len(sys.argv) > 2 else 4
ml = int(sys.argv[3]) if len(sys.argv) > 3 else 60
top = parse(data)
print(sys.argv[1], len(data), 'bytes;', 'top level parses' if top is not None else 'TOP LEVEL DOES NOT PARSE', '; first bytes', data[:16].hex(' '))
if top:
    show(top, 0, md, ml, [])
