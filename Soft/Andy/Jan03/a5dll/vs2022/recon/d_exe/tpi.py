"""Does the PDB carry full type records (TPI) for the storage classes? Print LF_MEMBER names of their field lists."""
import sys, struct, re
from pdb import PDB

p = PDB(sys.argv[1])
tpi = p.stream(2)
hdr_size, tmin, tmax, recbytes = struct.unpack_from('<IIII', tpi, 4)
print('streams', len(p.streams), 'TPI bytes', len(tpi), 'type records', tmax - tmin)
pos = hdr_size
recs = {}
ti = tmin
while pos < len(tpi) and ti < tmax:
    ln, kind = struct.unpack_from('<HH', tpi, pos)
    recs[ti] = (kind, tpi[pos + 4:pos + 2 + ln])
    pos += 2 + ln
    ti += 1


def leaf_num(b, i):
    v = struct.unpack_from('<H', b, i)[0]
    if v < 0x8000:
        return v, i + 2
    size = {0x8000: 1, 0x8001: 2, 0x8002: 2, 0x8003: 4, 0x8004: 4}.get(v, 4)
    return int.from_bytes(b[i + 2:i + 2 + size], 'little'), i + 2 + size


def members(fl):
    kind, b = recs[fl]
    out, i = [], 0
    while i < len(b):
        k = struct.unpack_from('<H', b, i)[0]
        if k == 0x150D:      # LF_MEMBER: attr(2) type(4) offset(num) name
            typ = struct.unpack_from('<I', b, i + 4)[0]
            off, j = leaf_num(b, i + 8)
            name = b[j:b.index(b'\0', j)].decode('latin-1')
            out.append((off, name, typ))
            i = b.index(b'\0', j) + 1
        elif k in (0x1510, 0x1511, 0x150E, 0x1409, 0x150F, 0x1400, 0x1401, 0x1404):
            # static member / method / onemethod / vfunctab / nested / bclass ... : skip to next name terminator
            if k == 0x1400:   # LF_BCLASS attr type offset
                _, j = leaf_num(b, i + 8)
                i = j
            elif k == 0x1409:  # LF_VFUNCTAB pad type
                i += 8
            else:
                j = b.index(b'\0', i + 8 if k != 0x1510 else i + 8)
                i = j + 1
        elif k >= 0xF0:
            i += 1 if k < 0xF0 else (k & 0x0F) or 1
        else:
            i += 2
        while i < len(b) and b[i] >= 0xF0:
            i += b[i] & 0x0F
    return out


for want in sys.argv[2:]:
    for ti, (kind, b) in recs.items():
        if kind in (0x1504, 0x1505):   # LF_CLASS / LF_STRUCTURE
            count, prop, fl = struct.unpack_from('<HHI', b, 0)
            if prop & 0x80 or fl == 0:  # forward ref
                continue
            size, j = leaf_num(b, 16)
            name = b[j:b.index(b'\0', j)].decode('latin-1')
            if name == want:
                print('%s: size %d, %d fields' % (name, size, count))
                for off, n, t in members(fl):
                    print('   +%-4d %s' % (off, n))
                break
