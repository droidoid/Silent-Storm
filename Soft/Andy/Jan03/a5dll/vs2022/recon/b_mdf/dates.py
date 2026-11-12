import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sys, mdf2000 as M
for fn in sys.argv[1:]:
    m, cols, objs, types, idx = M.load(fn)
    us = [o for o in objs.values() if (o['xtype'] or '').strip() != 'S' and o.get('crdate')]
    us.sort(key=lambda o: o['crdate'])
    print('==', fn, 'non-system objects:', len(us))
    print('  oldest:', [(str(o['crdate'])[:19], o['name']) for o in us[:3]])
    print('  newest:', [(str(o['crdate'])[:19], o['xtype'].strip(), o['name']) for o in us[-8:]])
    import collections
    c = collections.Counter(str(o['crdate'])[:7] for o in us)
    print('  by month:', sorted(c.items()))
