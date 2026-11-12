"""Decode rows of release game.db CDBTableDataStorage tables.
chunk 5: columns [1]{[2]=name,[3]=type 0 int/1 bool/2 float/3 string}; 6/7/8: int/float/string names.
chunk 2: per record [1]=count,[2]=packed dwords (ints in chunk-6 order, then bools); 3: floats; 4: wstrings.
usage: python rows.py game.db TABLEHEX [TABLEHEX...]  -> prints rows as tab-separated  (lib: rows(d,objs,id2ptr,tid))"""
import sys, struct
from ss import load, chunks
from tables import table_map, objects


def _names(d, s, e):
    return [d[ps:ps + ln].decode('cp1251') for c, ps, ln in chunks(d, s, e)]


def table(d, objs, id2ptr, tid):
    s, n = objs[id2ptr[tid]]
    parts = {c: (ps, ln) for c, ps, ln in chunks(d, s, s + n)}
    cols = []
    if 5 in parts:
        for c, ps, ln in chunks(d, *_se(parts[5])):
            nm, ty = None, None
            for c2, p2, l2 in chunks(d, ps, ps + ln):
                if c2 == 2:
                    nm = d[p2:p2 + l2].decode('cp1251')
                elif c2 == 3:
                    ty = struct.unpack_from('<i', d, p2)[0]
            cols.append((nm, ty))
    ints = _names(d, *_se(parts[6])) if 6 in parts else []
    flts = _names(d, *_se(parts[7])) if 7 in parts else []
    strs = _names(d, *_se(parts[8])) if 8 in parts else []
    bools = [nm for nm, ty in cols if ty == 1]
    recs_i = [list(chunks(d, ps, ps + ln)) for c, ps, ln in chunks(d, *_se(parts[2]))] if 2 in parts else []
    recs_f = [list(chunks(d, ps, ps + ln)) for c, ps, ln in chunks(d, *_se(parts[3]))] if 3 in parts else []
    recs_s = [list(chunks(d, ps, ps + ln)) for c, ps, ln in chunks(d, *_se(parts[4]))] if 4 in parts else []
    out = []
    for k in range(len(recs_i)):
        row = {}
        dw = []
        for c, ps, ln in recs_i[k]:
            if c == 2:
                dw = list(struct.unpack_from('<%di' % (ln // 4), d, ps))
        for nm, v in zip(ints + bools, dw):
            row[nm] = v
        if k < len(recs_f):
            for c, ps, ln in recs_f[k]:
                if c == 2:
                    fv = struct.unpack_from('<%df' % (ln // 4), d, ps)
                    for nm, v in zip(flts, fv):
                        row[nm] = v
        if k < len(recs_s):
            ss = [d[ps:ps + ln].decode('utf-16le', 'replace') for c, ps, ln in recs_s[k] if c == 1]
            for nm, v in zip(strs, ss):
                row[nm] = v
        out.append(row)
    return cols, out


def _se(p):
    return p[0], p[0] + p[1]


def open_db(path):
    d, top = load(path)
    ptr2id, _ = table_map(d, top)
    return d, objects(d, top), {v: k for k, v in ptr2id.items()}


if __name__ == '__main__':
    d, objs, id2ptr = open_db(sys.argv[1])
    for t in sys.argv[2:]:
        cols, rs = table(d, objs, id2ptr, int(t, 16))
        names = [c[0] for c in cols]
        print('### 0x%s cols=%s rows=%d' % (t, names, len(rs)))
        for r in rs:
            print('\t'.join(str(r.get(n, '')).replace('\r', '\\r').replace('\n', '\\n').replace('\t', '\\t') for n in names))
