"""Code drift Jan03 vs release: qualified function names (Scope::Name) from the release PDB publics vs the
external symbols defined by the Jan03 port objects.  usage: python drift.py <release.pdb> <build dir> <out dir>"""
import os, re, sys, glob, collections, struct
sys.path.insert(0, r'C:\Users\admin\SS\Silent-Storm\Soft\Andy\Jan03\a5dll\vs2022\recon\d_exe')
from pdb import PDB

PDBP, BLD, OUT = sys.argv[1], sys.argv[2], sys.argv[3]
os.makedirs(OUT, exist_ok=True)
BIG = bytes.fromhex('c7a1bad1eebaa94baf20faf66aa4dcb8')


def obj_defined(path):
    d = open(path, 'rb').read()
    if d[0:4] == b'\x00\x00\xff\xff' and d[12:28] == BIG:
        nsect, symptr, nsym = struct.unpack_from('<III', d, 44); ss, fmt = 20, '<8sIiHBB'
    else:
        _, nsect, _, symptr, nsym, _, _ = struct.unpack_from('<HHIIIHH', d, 0); ss, fmt = 18, '<8sIhHBB'
    strtab = symptr + nsym * ss
    i, out = 0, []
    while i < nsym:
        raw, val, sect, typ, cls, naux = struct.unpack_from(fmt, d, symptr + i * ss)
        i += 1 + naux
        if cls != 2 or sect <= 0 or (typ & 0x30) != 0x20:
            continue
        if raw[:4] == b'\0\0\0\0':
            p = strtab + struct.unpack_from('<I', raw, 4)[0]; name = d[p:d.index(b'\0', p)]
        else:
            name = raw.rstrip(b'\0')
        out.append(name.decode('latin-1'))
    return out


SIMPLE = re.compile(r'^\?([A-Za-z_]\w*)@((?:[A-Za-z_]\w*@)*)@')
CTOR = re.compile(r'^\?\?([01])((?:[A-Za-z_]\w*@)+)@')
SKIP_SCOPES = {'std', 'nstl', '_STL', 'stdext', 'ATL', 'Concurrency', 'type_info'}


def qual(sym):
    """mangled -> 'A::B::name' for plain functions/methods/ctors/dtors; None for templates/operators/C names"""
    m = SIMPLE.match(sym)
    if m:
        name, scopes = m.group(1), [s for s in m.group(2).split('@') if s]
    else:
        m = CTOR.match(sym)
        if not m:
            return None
        scopes = [s for s in m.group(2).split('@') if s]
        name = ('~' if m.group(1) == '1' else '') + scopes[0]
    if any(s in SKIP_SCOPES for s in scopes):
        return None
    return '::'.join(list(reversed(scopes)) + [name])


def strip_ns(q):
    return re.sub(r'^NDb::', '', q)


# release
rel = set()
for seg, off, name, fl in PDB(PDBP).publics():
    q = qual(name)
    if q:
        rel.add(q)
# Jan03: every object of the port build (engine, game, libraries)
jan = set()
objs = [p for p in glob.glob(os.path.join(BLD, 'CMakeFiles', '**', '*.obj'), recursive=True)
        if not re.search(r'Main_part_|DataImport|PkgBuilder|TexConv|TexMipStrip|FontGen|ADOImport|MemoryMngr', p)]
for o in objs:
    for s in obj_defined(o):
        q = qual(s)
        if q:
            jan.add(q)

reln = {strip_ns(q) for q in rel}
jann = {strip_ns(q) for q in jan}


def scope(q):
    return q.rsplit('::', 1)[0] if '::' in q else '<global>'


common = reln & jann
only_rel = reln - jann
only_jan = jann - reln
rs = collections.Counter(scope(q) for q in reln)
js = collections.Counter(scope(q) for q in jann)
new_scopes = {s for s in rs if s not in js}
gone_scopes = {s for s in js if s not in rs}
both_scopes = set(rs) & set(js)
new_in_old = [q for q in only_rel if scope(q) in both_scopes]
gone_in_old = [q for q in only_jan if scope(q) in both_scopes]

lines = []
lines.append('objects scanned: %d' % len(objs))
lines.append('functions (qualified, plain/ctor/dtor, no STL): release %d, Jan03 %d, common %d' % (len(reln), len(jann), len(common)))
lines.append('release-only %d (%.0f%% of release), Jan03-only %d' % (len(only_rel), 100.0 * len(only_rel) / len(reln), len(only_jan)))
lines.append('scopes (classes/namespaces): release %d, Jan03 %d, common %d, release-only %d, Jan03-only %d' % (
    len(rs), len(js), len(both_scopes), len(new_scopes), len(gone_scopes)))
lines.append('in common scopes: release adds %d functions, Jan03 has %d that release lacks' % (len(new_in_old), len(gone_in_old)))
# functions of release-only scopes
lines.append('functions living in release-only scopes: %d' % sum(rs[s] for s in new_scopes))
# grouping of new scopes by prefix
def group(s):
    s0 = s.split('::')[0]
    m = re.match(r'^(N[A-Z][a-z]+|C[A-Z][a-z]+|S[A-Z][a-z]+|I[A-Z][a-z]+|[A-Za-z]+)', s0)
    return m.group(1) if m else s0
lines.append('\nrelease-only scopes by top-level name (top 40, functions count):')
g = collections.Counter()
for s in new_scopes:
    g[s.split('::')[0]] += rs[s]
for k, v in g.most_common(40):
    lines.append('  %-40s %d' % (k, v))
lines.append('\ncommon scopes with most release-only functions (top 40):')
c = collections.Counter(scope(q) for q in new_in_old)
for k, v in c.most_common(40):
    lines.append('  %-50s +%d (release %d, Jan03 %d)' % (k, v, rs[k], js[k]))
lines.append('\nJan03-only scopes (top 30):')
for s in sorted(gone_scopes, key=lambda s: -js[s])[:30]:
    lines.append('  %-50s %d' % (s, js[s]))
open(os.path.join(OUT, 'drift.txt'), 'w').write('\n'.join(lines) + '\n')
open(os.path.join(OUT, 'release_only_functions.txt'), 'w').write('\n'.join(sorted(only_rel)) + '\n')
open(os.path.join(OUT, 'jan03_only_functions.txt'), 'w').write('\n'.join(sorted(only_jan)) + '\n')
print('\n'.join(lines[:8]))
