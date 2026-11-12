"""Static check of Complete resources (copied to samples/) against the Jan03 readers.
usage: python -I check.py [Type ...]
Schemas transcribe the Jan03 Add()/operator& calls; see comments for source locations."""
import os, re, sys, struct
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from ss import *

SRC = r'C:\Users\admin\SS\Silent-Storm\Soft\Andy\Jan03\a5dll'
SAMPLES = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'samples')

def jan03_class_ids():
    ids = {}
    rx = re.compile(r'REGISTER_SAVELOAD_(?:TEMPL_)?CLASS(?:_NM)?\(\s*(0x[0-9A-Fa-f]+|\d+)\s*,\s*([\w:<>, ]+?)\s*[,)]')
    for root, _, files in os.walk(SRC):
        if 'vs2022' in root:
            continue
        for f in files:
            if f.lower().endswith(('.cpp', '.h')):
                try:
                    t = open(os.path.join(root, f), 'rb').read().decode('cp1251', 'replace')
                except OSError:
                    continue
                for m in rx.finditer(t):
                    ids[int(m.group(1), 0) & 0xffffffff] = (m.group(2), f)
    return ids

P = lambda n: ('pod', n)
DV = lambda n: ('dvec', n)
S = lambda d, name='': ('struct', d, name)
DBPTR = S({1: P(4)}, 'CDBPtr')                    # ADOImport/BasicDB.h CDBPtr::operator&
PTR = P(4)                                        # CPtr/CObj: DoPtr -> RawData 4 bytes

SCHEMAS = {
    # GObjectInfo.cpp CObjectInfoLoader::RecalcValue (1,2,3,5) + CObjectInfoPiecesLoader (4); SLoadVertex = 60 bytes
    'Geometries': S({1: DV(60), 2: DV(2), 3: DV(2), 5: DV(12),
                     4: ('hmap', P(4), S({1: DV(60), 2: DV(2), 3: DV(2)}, 'SFacesVector'))}),
    # GAnimFormat.cpp CFileSkeleton::Recalc; GAnimFormat.h SBone
    'Skeletons': S({1: ('vec', S({1: ('str',), 2: P(4), 3: P(12), 4: P(16), 5: P(12)}, 'SBone')), 2: P(1)}),
    # CFileAnimation::Recalc; SRootAnimKey 28, SBoneAnimKey 16, SAddBoneAnimKey 32, SMSRAnimKey 40
    'Animations': S({1: S({1: DV(4), 2: P(4), 3: P(4), 4: P(4), 5: P(4), 6: P(4), 7: P(1)}, 'SAnimationHeader'),
                     2: DV(28), 3: DV(16), 4: DV(32), 5: DV(40)}),
    # METerrain.h CMETerrainInfo::operator& ; TerrainInfo.cpp STerrainInfo::operator&
    'Terrain': S({21: S({1: P(4), 2: P(4),
                         3: ('vec', S({1: P(4), 2: P(1), 4: DV(8)}, 'STerrainHole')),
                         4: ('a2d', 1), 5: ('a2d', 2),
                         6: ('vec', S({2: P(4), 3: ('a2d', 1), 4: P(4), 5: ('a2d', 4), 6: DV(8), 7: P(4)}, 'SGrassLayer')),
                         7: ('a2d', 4),
                         8: ('vec', S({2: DBPTR, 3: P(8), 4: P(8), 5: P(4), 6: S({2: DBPTR})}, 'STerrainSpot')),
                         9: P(4), 10: ('a2dobj', S({2: DBPTR}, 'SStepSound')), 11: ('a2d', 4)}, 'STerrainInfo'),
                  6: ('a2d', 2), 10: P(4), 11: P(4)}),
    # BuildingInfo.h CBuildInfo / SBuildFragment (BuildingInfo.cpp)
    'Buildings': None,  # filled below
    # aiObjectLoader.cpp CLoadGeometryInfo::Recalc (STriangle = 3 WORD = 6, SMassSphere 20, SJunction 16)
    'AIGeometries': S({1: DV(12), 2: DV(6),
                       4: ('hmap', P(4), S({1: DV(12), 2: DV(6), 10: P(4), 11: DV(16), 12: ('vec', PTR)}, 'SStoredPiece')),
                       6: DV(20), 8: ('vec', PTR)}),
    # ChapterInfo.h
    'Chapters': S({2: P(4), 3: P(8), 4: ('vec', S({2: P(4), 3: P(4), 4: P(4), 5: DV(8), 6: P(4), 7: ('str',)}, 'SChapterSector'))}),
}
RAWAPPLY = S({2: P(4), 3: P(4), 4: P(4), 5: P(8), 6: P(4)}, 'SRawMaterialApply')
FRAG = {2: P(4), 3: P(12), 4: P(4), 5: P(4), 6: P(4), 30: P(4), 31: DV(4), 32: P(4)}
for i in range(8):                                   # NDb::N_CONSTRUCTION_MATERIALS = 8
    FRAG[10 + i] = S({2: ('vec', RAWAPPLY)}, 'SRawMixedMaterial')
SCHEMAS['Buildings'] = S({2: ('vec', S(FRAG, 'SBuildFragment')), 3: ('vec', S(FRAG, 'SBuildFragment')),
                          4: ('vec', ('a2d', 1)),
                          5: ('vec', S({2: P(12), 3: P(12), 4: P(8), 5: P(4), 6: P(4), 7: P(4), 8: P(4)}, 'SProjectedSpot')),
                          6: P(4), 7: P(4), 8: P(4), 9: P(4),
                          10: ('vec', S({2: P(4), 3: S({2: P(12), 3: P(4), 4: PTR}, 'SDiscretePos'), 4: P(4), 5: P(4), 6: P(4), 7: P(4)}, 'SLadder')),
                          11: ('a2d', 1),
                          12: ('vec', S({2: DV(4), 3: DV(4)}, 'SLayerGroup')),
                          13: P(4), 14: P(4), 15: P(4)})
# alternative view of AIGeometries used for skins (CFileSkinPointsLoad: 4 = hash<int,SStoredSkin>, 6 = spheres)
AISKIN = S({4: ('hmap', P(4), S({1: DV(12), 2: DV(6), 3: DV(12)}, 'SStoredSkin')), 6: DV(20)})

def check_textures(files):
    print('Textures: Jan03 GTexture.cpp reads SMMPFileHeader{sig,format,avgColor,sizeX,sizeY,mips} (24 bytes) then mip data;'
          ' no signature check')
    blk = {1: 8, 2: 16, 3: 16, 4: 16, 5: 16}
    bpp = {6: 4, 7: 2, 8: 2, 9: 2}
    for fn, data in files:
        sig, fmt, avg, sx, sy, mips = struct.unpack_from('<IiIiii', data, 0)
        need = 24
        for l in range(mips):
            if fmt in blk:
                need += ((sx >> l) // 4) * ((sy >> l) // 4) * blk[fmt]
            elif fmt in bpp:
                need += (sx >> l) * (sy >> l) * bpp[fmt]
        print('  %-9s %9d B sig=0x%08X(%s) fmt=%d %dx%d mips=%d -> Jan03 consumes %d bytes (%s)' % (
            fn, len(data), sig, 'MMP' if sig == 0x00504D4D else 'NOT MMP', fmt, sx, sy, mips, need,
            'exact' if need == len(data) else 'diff %+d' % (len(data) - need)))

def check_effects(files):
    print('Effects: Jan03 GParticleFormat.cpp: int nBytes; float fTEnd, fFrameRate; int nParticles; SParticle[n] (34 bytes, pack 2),'
          ' track key offsets relative to byte 4')
    ksz = {'pos': 14, 'rot': 6, 'scale': 10, 'color': 6, 'sprite': 4}   # TKey<T> pack 2: short + value
    for fn, data in files:
        nbytes, tend, fps, npart = struct.unpack_from('<iffi', data, 0)
        base = 4
        bad = 0; maxend = 16 + npart * 34
        for k in range(min(npart, 100000)):
            o = 16 + k * 34
            if o + 34 > len(data):
                bad += 1; break
            ts, te = struct.unpack_from('<hh', data, o)
            for j, name in enumerate(['pos', 'rot', 'scale', 'color', 'sprite']):
                n, off = struct.unpack_from('<hi', data, o + 4 + j * 6)
                end = base + off + n * ksz[name]
                if n < 0 or off < 0 or end > len(data):
                    bad += 1
                maxend = max(maxend, end)
        print('  %-5s %8d B nBytes=%d (size-4=%d) tEnd=%g fps=%g nParticles=%d particle table end=%d, key data end=%d, bad tracks=%d' % (
            fn, len(data), nbytes, len(data) - 4, tend, fps, npart, 16 + npart * 34, maxend, bad))

def main():
    types = sys.argv[1:] or ['Geometries', 'Skeletons', 'Animations', 'Textures', 'Terrain', 'Buildings', 'AIGeometries', 'Effects', 'Chapters']
    reg = jan03_class_ids()
    print('Jan03 registered saveload class ids: %d' % len(reg))
    for t in types:
        d = os.path.join(SAMPLES, t)
        files = sorted(((f, open(os.path.join(d, f), 'rb').read()) for f in os.listdir(d)), key=lambda x: len(x[1]))
        print('=' * 100)
        if t == 'Textures':
            check_textures(files); continue
        if t == 'Effects':
            check_effects(files); continue
        schemas = [('main', SCHEMAS[t])]
        if t == 'AIGeometries':
            schemas.append(('skin-view', AISKIN))
        for sname, sch in schemas:
            rep = Report()
            print('%s [%s]: %d files' % (t, sname, len(files)))
            for fn, data in files:
                ch, used = parse_lenient(data)
                topids = [(c, len(p)) for c, p in ch]
                objs = object_table(data)
                ocls = sorted(set(o[0] for o in objs))
                unk = [hex(c) for c in ocls if c not in reg]
                r1 = Report()
                res = check_file(data, sch, r1)
                for a in ('missing', 'unknown'):
                    for k, v in getattr(r1, a).items():
                        rep.add(getattr(rep, a), k, v)
                for k, v in r1.sizebad.items():
                    rep.sizebad.setdefault(k, []).extend(v)
                rep.notes += ['%s: %s' % (fn, n) for n in r1.notes]
                cnt = {k: v for k, v in r1.counts.items() if k.count('.') == 1}
                print('  %-7s %8d B top=%s objs=%d classes=%s%s | miss=%d unk=%d sizebad=%d | %s' % (
                    fn, len(data), topids, len(objs), [hex(c) for c in ocls][:4],
                    (' UNKNOWN-TO-JAN03=' + str(unk)) if unk else '', sum(r1.missing.values()), sum(r1.unknown.values()),
                    sum(len(v) for v in r1.sizebad.values()), ' '.join('%s=%s' % (k[5:], v[0] if len(v) == 1 else v) for k, v in sorted(cnt.items()))))
            print(summarize(rep, len(files)))

if __name__ == '__main__':
    main()
