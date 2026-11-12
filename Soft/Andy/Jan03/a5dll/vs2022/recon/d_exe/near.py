"""Show 32-bit immediates near a VA that point into .text, resolved to public symbols."""
import sys, struct, bisect
from pdb import PDB, sections

exe, pdbp, va0 = sys.argv[1], sys.argv[2], int(sys.argv[3], 16)
span = int(sys.argv[4]) if len(sys.argv) > 4 else 40
d, base, secs = sections(exe)
pubs = sorted((seg, off, name) for seg, off, name, _ in PDB(pdbp).publics())
keys = [(s, o) for s, o, _ in pubs]


def va2raw(va):
    for si, (sn, sva, vs, rp, rs) in enumerate(secs, 1):
        if base + sva <= va < base + sva + max(vs, rs):
            return si, va - base - sva, rp + (va - base - sva)
    return None


def name_at(va):
    r = va2raw(va)
    if not r:
        return None
    si, off, _ = r
    k = bisect.bisect_right(keys, (si, off)) - 1
    if k >= 0 and pubs[k][0] == si:
        return '%s+0x%X' % (pubs[k][2], off - pubs[k][1])
    return None


_, _, raw0 = va2raw(va0)
print(d[raw0 - span:raw0 + span].hex(' '))
for i in range(raw0 - span, raw0 + span):
    v = struct.unpack_from('<I', d, i)[0]
    n = name_at(v) if v > base else None
    if n:
        print('  @va 0x%08X imm 0x%08X -> %s' % (va0 + (i - raw0), v, n))
