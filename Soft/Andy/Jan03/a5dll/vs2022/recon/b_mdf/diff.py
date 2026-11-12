# Compare code-expected tables/fields with an MDF schema dump. Usage: python -I diff.py code_expected.json mdf_schema_X.json out.txt
import sys, json
code = json.load(open(sys.argv[1]))
mdf = json.load(open(sys.argv[2]))
out = sys.argv[3]
L = []
ct = code['tables']
mt_lower = {k.lower(): k for k in mdf}
expected = list(ct) + code['relations']
missing = [t for t in expected if t not in mdf]
missing_ci = [t for t in missing if t.lower() not in mt_lower]
L.append('Code registers %d tables + %d relation table(s); MDF has %d user tables' % (len(ct), len(code['relations']), len(mdf)))
L.append('')
L.append('== Tables expected by code but MISSING in MDF (%d): %s' % (len(missing_ci), ', '.join(missing_ci)))
casediff = [t for t in missing if t not in missing_ci]
if casediff:
    L.append('   (present with different case: %s)' % ', '.join('%s->%s' % (t, mt_lower[t.lower()]) for t in casediff))
used = set(t.lower() for t in expected)
unused = sorted([t for t in mdf if t.lower() not in used], key=str.lower)
tree = [t for t in unused if t.lower().endswith('tree')]
other = [t for t in unused if not t.lower().endswith('tree')]
L.append('')
L.append('== MDF tables NOT referenced by code (%d): %d are *Tree (editor folder tables), %d others:' % (len(unused), len(tree), len(other)))
L.append('   *Tree: ' + ', '.join(tree))
L.append('   others: ' + ', '.join('%s(%d rows)' % (t, mdf[t]['rows']) for t in other))
L.append('')
L.append('== Column comparison for overlapping tables (code field -> MDF column; exact-case match required by COLETable::GetFieldIndex std::find)')
nmiss_total = 0; ncase_total = 0; ok_tables = 0; probs = []
for t in sorted(ct, key=str.lower):
    mk = t if t in mdf else mt_lower.get(t.lower())
    if not mk:
        continue
    cols = {c[0]: c for c in mdf[mk]['cols']}
    cols_l = {c.lower(): c for c in cols}
    miss = []; case = []
    for f in ct[t]['fields']:
        if f in cols:
            continue
        if f.lower() in cols_l:
            case.append('%s(MDF:%s)' % (f, cols_l[f.lower()]))
        else:
            miss.append(f)
    extra = [c for c in cols if c.lower() not in set(x.lower() for x in ct[t]['fields'])]
    nmiss_total += len(miss); ncase_total += len(case)
    status = 'OK' if not miss and not case else 'MISMATCH'
    if status == 'OK':
        ok_tables += 1
    else:
        probs.append(t)
    L.append('%-28s %-8s rows=%-6d codeFields=%-3d mdfCols=%-3d missingInMDF=%s%s  unusedMDFcols=%d' % (
        t, status, mdf[mk]['rows'], len(ct[t]['fields']), len(cols), miss, (' caseDiff=%s' % case) if case else '', len(extra)))
for r in code['relations']:
    if r in mdf:
        cols = [c[0] for c in mdf[r]['cols']]
        L.append('%-28s RELATION first two cols=%s (code maps <X>ID -> table <X>s)' % (r, cols[:2]))
L.append('')
L.append('SUMMARY: overlapping registered tables=%d, fully matching=%d, with problems=%d; missing fields total=%d, case-only diffs=%d; missing tables=%d' % (
    ok_tables + len(probs), ok_tables, len(probs), nmiss_total, ncase_total, len(missing_ci)))
with open(out, 'w', encoding='utf-8') as f:
    f.write('\n'.join(L))
print('\n'.join([l for l in L if not l.startswith(' ') or True][:6]))
print(L[-1])
