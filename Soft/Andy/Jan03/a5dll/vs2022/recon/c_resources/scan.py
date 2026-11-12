"""Population scan (read-only) of Complete\<Type>\* against the Jan03 schemas of check.py.
usage: python -I scan.py <Type> [limit]
Per file: structural signature (top-level ids | main-level ids) and whether the Jan03 schema matches
(no size mismatch, no unknown ids at main level). Prints signature histogram with example ids."""
import os, sys, struct, collections
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from ss import *
from check import SCHEMAS, jan03_class_ids

COMPLETE = r'C:\Users\admin\SS\Silent-Storm\Complete'
t = sys.argv[1]
limit = int(sys.argv[2]) if len(sys.argv) > 2 else 10 ** 9
d = os.path.join(COMPLETE, t)
names = sorted((f for f in os.listdir(d) if os.path.isfile(os.path.join(d, f))), key=lambda x: int(x) if x.isdigit() else -1)[:limit]
reg = jan03_class_ids()
sigs = collections.Counter(); ex = collections.defaultdict(list); verdict = collections.Counter()
detail = collections.defaultdict(collections.Counter)
cls_unknown = collections.Counter()
for fn in names:
    p = os.path.join(d, fn)
    if t == 'Textures':
        with open(p, 'rb') as f:
            h = f.read(24)
        size = os.path.getsize(p)
        sig, fmt, avg, sx, sy, mips = struct.unpack('<IiIiii', h)
        blk = {1: 8, 2: 16, 3: 16, 4: 16, 5: 16}; bpp = {6: 4, 7: 2, 8: 2, 9: 2}
        need = 24
        for l in range(max(0, min(mips, 16))):
            need += ((sx >> l) // 4) * ((sy >> l) // 4) * blk[fmt] if fmt in blk else (sx >> l) * (sy >> l) * bpp.get(fmt, 0)
        k = ('MMP' if sig == 0x00504D4D else 'sig%08X' % sig, 'fmt%d' % fmt, 'exact' if need == size else 'size-mismatch', 'id>=0x1000000' if int(fn) >= 0x1000000 else 'id<0x1000000')
        sigs[k] += 1
        if len(ex[k]) < 4: ex[k].append(fn)
        continue
    data = open(p, 'rb').read()
    if t == 'Effects':
        nbytes, tend, fps, npart = struct.unpack_from('<iffi', data, 0) if len(data) >= 16 else (-1, 0, 0, 0)
        ok = nbytes == len(data) - 4 and 16 + npart * 34 <= len(data)
        k = ('nBytes==size-4' if nbytes == len(data) - 4 else 'nBytes MISMATCH', 'table fits' if ok else 'BAD')
        sigs[k] += 1
        if len(ex[k]) < 4: ex[k].append(fn)
        continue
    ch, used = parse_lenient(data)
    top_ids = tuple(c for c, _ in ch)
    main = nth(ch, 1)
    mch = parse_lenient(main)[0] if main is not None else []
    main_ids = tuple(sorted(set(c for c, _ in mch)))
    objs = object_table(data)
    oc = tuple(sorted(set(hex(o[0]) for o in objs)))
    for o in objs:
        if o[0] not in reg:
            cls_unknown[hex(o[0])] += 1
    rep = Report()
    check_file(data, SCHEMAS[t], rep)
    main_unknown = [k for k in rep.unknown if k.count('.') == 1]
    deep_unknown = [k for k in rep.unknown if k.count('.') > 1 or '/' in k or '[' in k]
    good = not rep.sizebad and not main_unknown and used == len(data) and main is not None
    k = (top_ids, main_ids, oc, 'schema-OK' if good else 'MISMATCH')
    sigs[k] += 1
    verdict['schema-OK' if good else 'MISMATCH'] += 1
    for kk in list(rep.sizebad) + main_unknown + deep_unknown:
        detail[k][kk] += 1
    for kk in rep.missing:
        detail[k]['missing ' + kk] += 1
    if len(ex[k]) < 4: ex[k].append(fn)
print('%s: %d files scanned' % (t, len(names)))
if verdict: print('  verdict counts:', dict(verdict))
for k, n in sigs.most_common():
    print('  %5d  %s  e.g. %s' % (n, k, ex[k]))
    if detail.get(k):
        print('         issues: ' + ', '.join('%s:%d' % kv for kv in detail[k].most_common(10)))
if cls_unknown:
    print('  object classes unknown to Jan03:', dict(cls_unknown))
