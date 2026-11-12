"""Copy variant samples (found by scan.py / geomvar.py) into samples2/<Type>/ (read-only on Complete)."""
import os, shutil
SRC = r'C:\Users\admin\SS\Silent-Storm\Complete'
DST = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'samples2')
pick = {
    'Geometries': ['2', '10', '100', '131101', '131314', '196700', '131747'],   # 56-byte verts variants + one 1,2,3,4 file
    'Terrain': ['86', '127', '300', '315', '8017'],                             # flattened / mixed / top-4
    'Chapters': ['1', '6', '3', '22'],                                          # new layout / top-4
    'AIGeometries': ['72', '129', '1', '22', '66848'],                         # old BSP chunk 8 / misc
    'Buildings': ['742', '686', '796', '5143', '218'],                         # legacy variants
}
for t, ids in pick.items():
    os.makedirs(os.path.join(DST, t), exist_ok=True)
    for i in ids:
        s = os.path.join(SRC, t, i)
        if os.path.exists(s):
            shutil.copyfile(s, os.path.join(DST, t, i))
            print(t, i, os.path.getsize(s))
