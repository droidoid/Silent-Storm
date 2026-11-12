"""Read int/bool columns of release game.db tables (CDBTableDataStorage): chunk 6 = int column names?, chunk 2 = records."""
import sys, struct
from ss import load, chunks
from tables import table_map, objects

def kids(d, s, n): return list(chunks(d, s, s + n))
def table(d, objs, id2ptr, tid):
    s, n = objs[id2ptr[tid]]
    ch = {}
    for c, ps, ln in kids(d, s, n): ch.setdefault(c, []).append((ps, ln))
    names = {}
    for c in (6, 7, 8):
        if c in ch:
            ps, ln = ch[c][0]
            names[c] = [d[p2:p2 + l2] for _, p2, l2 in kids(d, ps, ln)]
    recs = []
    ps, ln = ch[2][0]
    for _, rs, rl in kids(d, ps, ln):
        sub = {c: (p, l) for c, p, l in kids(d, rs, rl)}
        cnt = struct.unpack_from('<i', d, sub[1][0])[0] if 1 in sub else 0
        dw = struct.unpack_from('<%di' % cnt, d, sub[2][0]) if 2 in sub and cnt else ()
        recs.append(dw)
    return names, recs

if __name__ == '__main__':
    d, top = load(sys.argv[1]); ptr2id, _ = table_map(d, top); id2ptr = {v: k for k, v in ptr2id.items()}; objs = objects(d, top)
    for t in sys.argv[2:]:
        names, recs = table(d, objs, id2ptr, int(t, 16))
        print('table', t, {k: [x[:40] for x in v][:12] for k, v in names.items()}, 'records', len(recs), 'first', recs[:2])
