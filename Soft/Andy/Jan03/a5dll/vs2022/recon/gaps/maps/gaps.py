"""Static check of the remaining map resource types (copied to gaps_m/samples) against the Jan03 readers.
usage: python -I gaps.py [Type ...]   -> per type: layout histogram, schema verdict, class ids unknown to Jan03.
Schemas transcribe Jan03 operator&/Add() (see comments). ss.py/check.py are copies of C:\a5b\recon\c."""
import os, sys, struct, collections
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from ss import *
from check import jan03_class_ids, SCHEMAS as OLD

SAMPLES = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'samples')
P = lambda n: ('pod', n); DV = lambda n: ('dvec', n); S = lambda d, name='': ('struct', d, name); PTR = P(4)
SBONE = OLD['Skeletons'][1][1]                      # vec SBone (GAnimFormat.h)
SCHEMAS = {
    # aiWaypoint.h CWaypoint / SCommand (ECommand 4, CVec3 12, STime=DWORD 4, EPose 4, EDirection 4)
    'Waypoints': S({2: P(12), 3: P(4), 4: P(4), 5: ('vec', S({2: P(4), 3: P(12), 4: P(4), 5: P(4), 6: P(4)}, 'SCommand'))}),
    # aiWaypoint.h CUnitAIInfo { vector<SRoute{vector<int>}> }
    'Units': S({2: ('vec', S({2: DV(4)}, 'SRoute'))}),
    'Groups': S({2: ('vec', S({2: DV(4)}, 'SRoute'))}),
    # GlobalInfo.h
    'Globals': S({2: P(4), 3: P(4), 4: ('vec', S({2: P(4), 3: P(4), 4: P(4), 5: P(8), 6: DV(8)}, 'SGlobalSector'))}),
    # GBind.cpp: Add(4, vector<SHMatrix>) (64 B)
    'Binds': S({4: DV(64)}), 'AIBinds': S({4: DV(64)}),
    # GAnimFormat.cpp CFileLocators: Add(1, bones)
    'Locators': S({1: SBONE}),
    # GAnimLight.cpp: 1..3 float, 4/5 TKeyVector<CVec3>{2: vector<TKey> pack2 = 14 B}, 6 TKeyVector<float> (6 B)
    'Lights': S({1: P(4), 2: P(4), 3: P(4), 4: S({2: DV(14)}), 5: S({2: DV(14)}), 6: S({2: DV(6)})}),
    # aiObjectLoader.cpp CLoadTwoBSPTrees: 1/2 = vector<hash_map<int,CPtr<CBSPTree>>>
    'AIBSPTrees': S({1: ('vec', ('hmap', P(4), PTR)), 2: ('vec', ('hmap', P(4), PTR))}),
    # LSHead.cpp CHeadMeshLoader: 1 vector<CMemoryStream>, 2 vector<int>, 3 vector<CTPoint<int>>, 4 vector<CVec2>, 5 vector<WORD>, 6 vector<int>
    'Heads': S({1: ('vec', ('str',)), 2: DV(4), 3: DV(8), 4: DV(8), 5: DV(2), 6: DV(4)}),
    # LSHead.cpp CHeadSequenceLoader: 1 CMemoryStream
    'Sequences': S({1: ('str',)}),
    # GFont.cpp: Add(1, CObj<CFontFormatInfo>)
    'Fonts': S({1: PTR}),
}

def mmp(data):
    sig, fmt, avg, sx, sy, mips = struct.unpack_from('<IiIiii', data, 0)
    blk = {1: 8, 2: 16, 3: 16, 4: 16, 5: 16}; bpp = {6: 4, 7: 2, 8: 2, 9: 2}
    need = 24
    for l in range(max(0, min(mips, 16))):
        need += ((sx >> l) // 4) * ((sy >> l) // 4) * blk[fmt] if fmt in blk else (sx >> l) * (sy >> l) * bpp.get(fmt, 0)
    return ('MMP' if sig == 0x00504D4D else 'sig%08X' % sig, 'fmt%d' % fmt, 'exact' if need == len(data) else 'size%+d' % (len(data) - need)), (sx, sy, mips)

def files_of(t):
    d = os.path.join(SAMPLES, t)
    return sorted((f for f in os.listdir(d) if os.path.isfile(os.path.join(d, f))), key=lambda x: int(x) if x.isdigit() else -1), d

def run(t, reg):
    names, d = files_of(t)
    print('=' * 100); print('%s: %d files' % (t, len(names)))
    if t == 'LRTextures':
        h = collections.Counter(); ex = collections.defaultdict(list); dims = collections.Counter()
        for fn in names:
            data = open(os.path.join(d, fn), 'rb').read()
            k, dm = mmp(data); h[k] += 1; dims['%dx%d' % dm[:2]] += 1
            if len(ex[k]) < 4: ex[k].append(fn)
        for k, n in h.most_common(): print('  %5d %s e.g. %s' % (n, k, ex[k]))
        print('  sizes:', dims.most_common(6)); return
    sch = SCHEMAS[t]
    hist = collections.Counter(); ex = collections.defaultdict(list); tot = Report(); unk_cls = collections.Counter()
    nonempty = collections.Counter()
    for fn in names:
        data = open(os.path.join(d, fn), 'rb').read()
        ch, used = parse_lenient(data)
        top_ids = tuple(c for c, _ in ch)
        main = nth(ch, 1)
        mch = parse_lenient(main)[0] if main is not None else []
        main_ids = tuple(c for c, _ in mch)
        objs = object_table(data)
        ocls = tuple(sorted(set(hex(o[0]) for o in objs)))
        for o in objs:
            if o[0] not in reg: unk_cls[hex(o[0])] += 1
        r = Report(); res = check_file(data, sch, r)
        for a in ('missing', 'unknown'):
            for k, v in getattr(r, a).items(): tot.add(getattr(tot, a), k, v)
        for k, v in r.sizebad.items(): tot.sizebad.setdefault(k, []).extend(v)
        tot.notes += ['%s: %s' % (fn, n) for n in r.notes]
        main_unknown = [k for k in r.unknown if k.count('.') == 1]
        ok = not r.sizebad and not main_unknown and not [k for k in r.missing if k.count('.') == 1] and used == len(data)
        # "data present": every first-level field chunk the schema wants is there and non-empty
        k = (top_ids if len(top_ids) < 8 else top_ids[:8] + ('...',), tuple(sorted(set(main_ids))), ocls, 'schema-OK' if ok else 'MISMATCH')
        hist[k] += 1
        if len(ex[k]) < 5: ex[k].append(fn)
        for fid in sch[1]:
            p = nth(mch, fid)
            if p: nonempty[fid] += 1
    for k, n in hist.most_common():
        print('  %5d top=%s main=%s classes=%s %s  e.g. %s' % (n, k[0], k[1], k[2], k[3], ex[k]))
    print('  first-level fields present & non-empty (of %d files): %s' % (len(names), dict(sorted(nonempty.items()))))
    print(summarize(tot, len(names)))
    if unk_cls: print('  CLASS IDS NOT REGISTERED IN JAN03:', dict(unk_cls))

if __name__ == '__main__':
    reg = jan03_class_ids()
    for t in sys.argv[1:] or ['Waypoints', 'Units', 'Groups', 'Globals', 'Binds', 'AIBinds', 'Locators', 'Lights', 'AIBSPTrees', 'Heads', 'Sequences', 'Fonts', 'LRTextures']:
        run(t, reg)
