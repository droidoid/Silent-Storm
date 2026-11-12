"""Print top-level CStructureSaver chunks (id, len) and version chunk 4 of given files. usage: python top_chunks.py <file>..."""
import struct, sys
def top(path):
    with open(path, 'rb') as f: d = f.read(64 * 1024 * 1024)
    pos, out = 0, []
    while pos < len(d) and len(out) < 10:
        cid = d[pos]; b = d[pos + 1]
        if b & 1: ln = struct.unpack_from('<I', d, pos + 1)[0] >> 1; hdr = 5
        else: ln = b >> 1; hdr = 2
        out.append((cid, ln, d[pos + hdr: pos + hdr + min(ln, 8)].hex()))
        pos += hdr + ln
    return out
for p in sys.argv[1:]:
    try: print(p, top(p))
    except Exception as e: print(p, 'ERR', e)
