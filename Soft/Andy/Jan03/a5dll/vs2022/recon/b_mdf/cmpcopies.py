import json, sys
names = ['Data', 'VersionsCurrent', 'ToolsBase_Data', 'ToolsBase_A5Game']
d = {n: json.load(open('mdf_schema_%s.json' % n)) for n in names}
L = []
for a, b in [('Data', 'VersionsCurrent'), ('ToolsBase_Data', 'Data'), ('ToolsBase_A5Game', 'ToolsBase_Data')]:
    A, B = d[a], d[b]
    L.append('== %s (%d tables) vs %s (%d tables)' % (a, len(A), b, len(B)))
    L.append('  only in %s: %s' % (a, sorted(set(A) - set(B), key=str.lower)))
    L.append('  only in %s: %s' % (b, sorted(set(B) - set(A), key=str.lower)))
    nd = 0; rowdiff = 0
    for t in sorted(set(A) & set(B), key=str.lower):
        ca = [tuple(c) for c in A[t]['cols']]; cb = [tuple(c) for c in B[t]['cols']]
        if A[t]['rows'] != B[t]['rows']: rowdiff += 1
        if ca != cb:
            nd += 1
            sa = {c[0]: c for c in ca}; sb = {c[0]: c for c in cb}
            L.append('  %-30s cols -%s +%s type/len changed %s' % (t, [x for x in sa if x not in sb], [x for x in sb if x not in sa],
                     [x for x in sa if x in sb and sa[x] != sb[x]]))
    L.append('  common tables: %d, with column differences: %d, with different row counts: %d' % (len(set(A) & set(B)), nd, rowdiff))
    tot = lambda X: sum(v['rows'] for v in X.values())
    L.append('  total rows: %s=%d %s=%d' % (a, tot(A), b, tot(B)))
open('copies_compare.txt', 'w', encoding='utf-8').write('\n'.join(L))
for l in L:
    if not l.startswith('  ') or 'common' in l or 'total' in l or 'only' in l: print(ascii(l)[:600])
