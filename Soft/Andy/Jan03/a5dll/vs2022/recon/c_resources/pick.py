"""Copy ~7 samples per resource type (size quantiles) from Complete into samples/<type>/ (read-only on source)."""
import os, shutil, sys
SRC = r'C:\Users\admin\SS\Silent-Storm\Complete'
DST = r'C:\a5b\recon\c\samples'
TYPES = ['Geometries', 'Skeletons', 'Animations', 'Textures', 'Terrain', 'Buildings', 'AIGeometries', 'Effects', 'Chapters']
for t in TYPES:
    d = os.path.join(SRC, t)
    files = [(os.path.getsize(os.path.join(d, f)), f) for f in os.listdir(d) if os.path.isfile(os.path.join(d, f))]
    files.sort()
    names = [f for _, f in files]
    ids = sorted(int(f) for f in names if f.isdigit())
    nondig = [f for f in names if not f.isdigit()]
    print('%-13s n=%d  id range %s..%s  ids>=65536: %d  non-numeric: %s' % (
        t, len(files), ids[0] if ids else '-', ids[-1] if ids else '-', sum(1 for i in ids if i >= 65536), nondig[:5]))
    n = len(files)
    pick = sorted(set(min(n - 1, int(q * (n - 1))) for q in (0, 0.1, 0.25, 0.5, 0.75, 0.9, 1.0)))
    od = os.path.join(DST, t)
    os.makedirs(od, exist_ok=True)
    for i in pick:
        sz, f = files[i]
        shutil.copyfile(os.path.join(d, f), os.path.join(od, f))
        print('   ', f, sz)
# packs
od = os.path.join(DST, 'res')
os.makedirs(od, exist_ok=True)
for f in os.listdir(SRC):
    if f.lower().endswith('.res'):
        shutil.copyfile(os.path.join(SRC, f), os.path.join(od, f))
        print('pack', f, os.path.getsize(os.path.join(SRC, f)))
