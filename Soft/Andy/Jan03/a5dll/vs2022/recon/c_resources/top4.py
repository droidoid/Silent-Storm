"""Show top-level chunk 4 (not written by Jan03 CStructureSaver) in a few release files (read-only)."""
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from ss import parse_lenient
C = r'C:\Users\admin\SS\Silent-Storm\Complete'
for t, f in [('Terrain', '8017'), ('Buildings', '5143'), ('Chapters', '3'), ('Chapters', '22'), ('AIGeometries', '1673'), ('AIGeometries', '993'), ('AIGeometries', '2418')]:
    d = open(os.path.join(C, t, f), 'rb').read()
    ch, _ = parse_lenient(d)
    print(t, f, [c for c, _ in ch], [(c, p.hex()) for c, p in ch if c == 4])
