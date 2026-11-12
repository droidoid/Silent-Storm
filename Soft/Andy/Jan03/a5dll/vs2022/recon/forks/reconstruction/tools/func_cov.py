"""Estimate which release-only qualified functions (from PDB) appear as definitions in a source tree.
usage: python func_cov.py <release_only_functions.txt> <out_prefix> <name=root> [<name=root> ...]
strict: 'Class::method(' (or 'Ns::func(' / bare free func defined at column 0) found;
loose : strict OR (class/struct declared AND 'method(' token appears in a file declaring the class)."""
import os, re, sys, collections
rel = [l.strip() for l in open(sys.argv[1], encoding='latin-1') if l.strip()]
outp = sys.argv[2]
roots = [a.split('=', 1) for a in sys.argv[3:]]
SUBS = ['NAI', 'NUI', 'NWorld', 'NGame', 'NRPG', 'NGScene', 'NScript']
PAIR = re.compile(r'\b(~?\w+)\s*::\s*(~?\w+)\s*\(')
FREEDEF = re.compile(r'^[A-Za-z_][\w:<>\*&, ]*?[\s\*&](\w+)\s*\([^;]*$', re.M)
CLS = re.compile(r'\b(?:class|struct)\s+(?:\w+\s+)?(\w+)\s*(?::|\{|$)', re.M)
IDCALL = re.compile(r'\b(~?\w+)\s*\(')
def index(root):
    pairs, frees, cls_files, file_ids = set(), set(), collections.defaultdict(set), {}
    for dp, dn, fn in os.walk(root):
        dn[:] = [d for d in dn if d not in ('.git', 'third_party', 'vs2022', 'build')]
        for f in fn:
            if not f.lower().endswith(('.cpp', '.h', '.hpp', '.inl', '.c')): continue
            p = os.path.join(dp, f)
            t = open(p, encoding='cp1251', errors='replace').read()
            t = re.sub(r'//[^\n]*', '', t)
            for a, b in PAIR.findall(t): pairs.add((a, b))
            for m in FREEDEF.findall(t): frees.add(m)
            for m in re.findall(r'BEGIN_SCRIPT_COMMAND\(\s*(\w+)', t): frees.add('lua' + m)
            ids = set(IDCALL.findall(t)); file_ids[p] = ids
            for c in CLS.findall(t): cls_files[c].add(p)
    return pairs, frees, cls_files, file_ids
def check(q, idx):
    pairs, frees, cls_files, file_ids = idx
    parts = q.split('::')
    name = parts[-1]
    if len(parts) >= 2:
        owner = parts[-2]
        if (owner, name) in pairs: return 'strict'
        if owner.startswith('N') and owner[1:2].isupper() and len(parts) == 2 and name in frees: return 'strict'
        if owner in cls_files:
            if any(name in file_ids[p] for p in cls_files[owner]): return 'loose'
            return 'class'
        return None
    return 'strict' if name in frees else None
res = {}
idxs = {}
for nm, root in roots:
    idxs[nm] = index(root)
out = open(outp + '_detail.tsv', 'w', encoding='utf-8')
out.write('qname\t' + '\t'.join(n for n, _ in roots) + '\n')
summ = collections.defaultdict(lambda: collections.Counter())
for q in rel:
    sub = q.split('::')[0] if q.split('::')[0] in SUBS else 'other'
    row = []
    for nm, _ in roots:
        r = check(q, idxs[nm]) or 'absent'
        row.append(r)
        summ[(nm, sub)][r] += 1; summ[(nm, 'ALL')][r] += 1
    out.write(q + '\t' + '\t'.join(row) + '\n')
lines = ['release-only qualified functions: %d' % len(rel), 'columns: strict = out-of-line Class::method( found; loose = class declared + method( token in a declaring file (inline); class = class exists, method not found; absent = class/func not found', '']
for nm, _ in roots:
    lines.append('== tree: %s' % nm)
    lines.append('%-8s %6s %7s %6s %6s %7s  %s' % ('subsys', 'total', 'strict', 'loose', 'class', 'absent', 'strict+loose %'))
    for sub in SUBS + ['other', 'ALL']:
        c = summ[(nm, sub)]; tot = sum(c.values())
        if not tot: continue
        lines.append('%-8s %6d %7d %6d %6d %7d  %5.1f%%' % (sub, tot, c['strict'], c['loose'], c['class'], c['absent'], 100.0 * (c['strict'] + c['loose']) / tot))
    lines.append('')
open(outp + '_summary.txt', 'w').write('\n'.join(lines))
print('\n'.join(lines))
