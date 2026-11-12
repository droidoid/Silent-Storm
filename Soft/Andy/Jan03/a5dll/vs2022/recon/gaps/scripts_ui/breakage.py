"""Per DB script: which release-only C functions it calls (would raise 'call nil' in Jan03). -> script_breakage.tsv"""
import csv, os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from lua_calls import tokens
csv.field_size_limit(10 ** 9)
rows = list(csv.DictReader(open('script_calls.tsv'), delimiter='\t'))
rel = {r['name'] for r in rows if r['status'] == 'RELEASE-ONLY-C'}
refs = {l.split()[0] for l in open('script_refs.txt', encoding='utf-8-sig') if l.strip()}
n = k = kr = 0
out = open('script_breakage.tsv', 'w', encoding='utf-8')
for ln in open('db/E000000C_Scripts.tsv', encoding='utf-8').read().split('\n')[1:]:
    if not ln:
        continue
    p = ln.split('\t')
    code = p[2].replace('\\r', '\r').replace('\\n', '\n').replace('\\t', '\t').replace('\\\\', '\\')
    t = [x for x in tokens(code) if x[0] != 'cm']
    m = sorted({t[i][1] for i in range(len(t) - 1) if t[i][0] == 'id' and t[i + 1][1] == '(' and t[i][1] in rel
                and (i == 0 or t[i - 1][1] not in ('.', ':', 'function'))})
    r = p[0] in refs
    n += 1
    k += bool(m)
    kr += bool(m) and r
    out.write('%s\t%s\treferenced=%s\t%d\t%s\n' % (p[0], p[1], r, len(m), ' '.join(m)))
print('db scripts', n, 'with >=1 release-only C call:', k, 'of which referenced by data:', kr, '; referenced scripts total', len(refs))
