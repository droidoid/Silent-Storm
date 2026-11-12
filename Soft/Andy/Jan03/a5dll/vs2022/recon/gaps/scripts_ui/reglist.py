"""Read NScript::pRegList (array of {char *name, lua_CFunction f}) from a release exe using PDB publics.
usage: python reglist.py <exe> <pdb> > release_reglist.txt"""
import sys, struct, bisect
from pdb import PDB, sections

exe, pdbp = sys.argv[1], sys.argv[2]
d, base, secs = sections(exe)
pubs = PDB(pdbp).publics()
addr = {}
for seg, off, name, fl in pubs:
    addr[name] = (seg, off)
func_by_va = {}
for seg, off, name, fl in pubs:
    if 1 <= seg <= len(secs):
        func_by_va[base + secs[seg - 1][1] + off] = name


def va2raw(va):
    for sn, sva, vs, rp, rs in secs:
        if base + sva <= va < base + sva + max(vs, rs):
            return rp + va - base - sva
    return None


def cstr(va):
    r = va2raw(va)
    return d[r:d.index(b'\0', r)].decode('latin-1')


seg, off = addr['?pRegList@NScript@@3PAUSRegFunction@Script@@A']
va = base + secs[seg - 1][1] + off
r = va2raw(va)
n = 0
while True:
    pn, pf = struct.unpack_from('<II', d, r + 8 * n)
    if pn == 0 or pf == 0:
        break
    print('%s\t%s' % (cstr(pn), func_by_va.get(pf, '0x%08X' % pf)))
    n += 1
print('# entries:', n, file=sys.stderr)
