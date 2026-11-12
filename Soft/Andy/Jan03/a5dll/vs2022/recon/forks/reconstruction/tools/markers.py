"""Count reconstruction markers and TODO/FIXME/HACK in source trees. usage: python markers.py <out> name=root ..."""
import os, re, sys, collections
out = sys.argv[1]
PATS = {
 'addr @0x...': r'@\s*0x[0-9a-fA-F]{5,}',
 'any 0x addr (6+ hex)': r'0x[0-9a-fA-F]{6,}',
 'retail': r'\bretail\b',
 'release': r'\brelease\b',
 'PDB': r'\bPDB\b',
 'decompil*': r'decompil',
 'disasm': r'disasm',
 'RVA': r'\bRVA\b',
 'v1.1/v1.2': r'\bv1\.[12]\b',
 'Steam': r'\bSteam\b',
 'dev (vs retail)': r'\bdev\b',
 'recovered/restored': r'recover|restor',
 'TODO': r'\bTODO\b', 'FIXME': r'\bFIXME\b', 'HACK': r'\bHACK\b', 'XXX': r'\bXXX\b',
 'stub/not implemented': r'\bstub|not implemented|unimplemented|NYI\b',
}
res = {}; top = {}
for spec in sys.argv[2:]:
    nm, root = spec.split('=', 1)
    c = collections.Counter(); files = collections.defaultdict(collections.Counter)
    for dp, dn, fn in os.walk(root):
        dn[:] = [d for d in dn if d not in ('.git', 'third_party', 'vs2022', 'build', 'zlib', 'libpng')]
        for f in fn:
            if not f.lower().endswith(('.cpp', '.h', '.c', '.hpp', '.inl')): continue
            p = os.path.join(dp, f)
            t = open(p, encoding='cp1251', errors='replace').read()
            com = ' '.join(re.findall(r'//[^\n]*|/\*.*?\*/', t, flags=re.S))
            for k, rx in PATS.items():
                n = len(re.findall(rx, com, flags=0 if k in ('PDB','RVA','TODO','FIXME','HACK','XXX','Steam') else re.I))
                c[k] += n; files[k][os.path.relpath(p, root)] += n
    res[nm] = c; top[nm] = files
L = ['counts inside comments (// and /* */), source files excluding third_party', '%-24s ' % 'marker' + ' '.join('%10s' % n for n in res)]
for k in PATS:
    L.append('%-24s ' % k + ' '.join('%10d' % res[n][k] for n in res))
for nm in res:
    for k in ('addr @0x...', 'TODO', 'stub/not implemented'):
        L.append('\n%s top files for %s: ' % (nm, k) + ', '.join('%s:%d' % kv for kv in top[nm][k].most_common(12) if kv[1]))
open(out, 'w').write('\n'.join(L) + '\n'); print('\n'.join(L))
