"""Coverage: release game.db record IDs vs resource files in Complete (names only, read-only listing) + hard-coded ids in Jan03 MapBuild.cpp."""
import os, sys
from ss import load
from tables import table_map, objects
from dbids import table
C = r'C:\Users\admin\SS\Silent-Storm\Complete'
d, top = load(os.path.join(C, 'game.db')); ptr2id, _ = table_map(d, top); id2ptr = {v: k for k, v in ptr2id.items()}; objs = objects(d, top)
def ids(tid, col=b'ID'):
    names, recs = table(d, objs, id2ptr, tid); i = names[6].index(col)
    return [r[i] for r in recs], names, recs
def files(t): return set(int(f) for f in os.listdir(os.path.join(C, t)) if f.isdigit())
for tid, tname, folder in [(60, 'Waypoints', 'Waypoints'), (11, 'Units', 'Units'), (69, 'Heads', 'Heads'), (70, 'HeadSeqs', 'Sequences'),
                           (74, 'Lights', 'Lights'), (61, 'Doors', 'AIBSPTrees'), (12, 'Fonts', 'Fonts'), (16, 'GlobalMaps', 'Globals'), (15, 'ChapterMaps', 'Chapters')]:
    try:
        rid, names, recs = ids(tid)
    except Exception as e:
        print('%s: %r' % (tname, e)); continue
    f = files(folder); r = set(rid)
    print('%-12s table %3d: %5d records; %-10s %5d files; records with file %5d; records WITHOUT file %5d %s; files without record %d %s' % (
        tname, tid, len(r), folder, len(f), len(r & f), len(r - f), sorted(r - f)[:8], len(f - r), sorted(f - r)[:8]))
    print('     int columns:', [x.decode('latin-1') for x in names[6]])
for tid, tname, want in [(9, 'Materials', 1904), (40, 'Objects(RndObject)', 1836)]:
    rid, _, _ = ids(tid); print('hard-coded id %d in %s (MapBuild.cpp): %s' % (want, tname, 'present' if want in rid else 'ABSENT'))
