"""Dump every release game.db table to db\<tid>_<name>.tsv (strings escaped)."""
import sys, os
from rows import open_db, table

names = {}
for line in open(os.path.join(os.path.dirname(__file__), 'release_columns.txt'), encoding='utf-8', errors='replace'):
    if line.startswith('0x'):
        p = line.split()
        names[int(p[0], 16)] = p[1]
d, objs, id2ptr = open_db(sys.argv[1])
out = sys.argv[2]
os.makedirs(out, exist_ok=True)
for tid in sorted(id2ptr):
    cols, rs = table(d, objs, id2ptr, tid)
    cn = [c[0] for c in cols]
    fn = os.path.join(out, '%08X_%s.tsv' % (tid, names.get(tid, '?').replace('?', 'unk')))
    with open(fn, 'w', encoding='utf-8') as f:
        f.write('\t'.join(cn) + '\n')
        for r in rs:
            f.write('\t'.join(str(r.get(n, '')).replace('\\', '\\\\').replace('\r', '\\r').replace('\n', '\\n').replace('\t', '\\t') for n in cn) + '\n')
    print('0x%08X %s rows=%d' % (tid, names.get(tid), len(rs)))
