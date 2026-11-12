"""Inspect Geometries files whose vertex chunk is not count*60 (read-only on Complete): element size histogram."""
import os, sys, collections
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from ss import parse_lenient, nth, i32
C = os.path.join(r'C:\Users\admin\SS\Silent-Storm\Complete', sys.argv[1] if len(sys.argv) > 1 else 'Geometries')
hist = collections.Counter(); ex = collections.defaultdict(list)
for f in os.listdir(C):
    d = open(os.path.join(C, f), 'rb').read()
    ch, _ = parse_lenient(d)
    m = nth(ch, 1)
    if m is None:
        continue
    mc = parse_lenient(m)[0]
    key = []
    for fid in (1, 2, 3, 5):
        p = nth(mc, fid)
        if p is None:
            key.append('%d:-' % fid); continue
        sc = parse_lenient(p)[0]
        n1 = [x for c, x in sc if c == 1]; n2 = [x for c, x in sc if c == 2]
        if len(n1) == 1 and len(n2) <= 1:
            cnt = i32(n1[0]) if len(n1[0]) == 4 else None
            dl = len(n2[0]) if n2 else 0
            es = (dl / cnt) if cnt else None
            key.append('%d:dvec*%s' % (fid, ('%g' % es) if es is not None else ('0' if not dl else 'cnt0+data')))
        else:
            key.append('%d:%dx1/%dx2' % (fid, len(n1), len(n2)))
    k = ' '.join(key)
    hist[k] += 1
    if len(ex[k]) < 4:
        ex[k].append((f, len(d)))
for k, n in hist.most_common():
    print('%5d  %s  e.g. %s' % (n, k, ex[k]))
