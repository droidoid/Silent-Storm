"""Per-module diff Jan03 -> Reconstruction (source files only).
usage: python module_diff.py <jan03 root> <recon root> <workdir> <out_prefix>
Normalizes encoding (utf-8 or cp1251 -> utf-8), CRLF -> LF, strips trailing whitespace, then git diff --no-index --numstat."""
import os, re, sys, subprocess, collections, shutil
J, R, W, OUT = sys.argv[1:5]
EXT = ('.cpp', '.h', '.c', '.hpp', '.inl')
SKIPD = {'.git', 'vs2022', 'third_party', 'build', 'zlib', 'libpng'}
def files(root):
    s = set()
    for dp, dn, fn in os.walk(root):
        dn[:] = [d for d in dn if d not in SKIPD]
        for f in fn:
            if f.lower().endswith(EXT):
                s.add(os.path.relpath(os.path.join(dp, f), root).replace(chr(92), '/'))
    return s
def norm(p):
    b = open(p, 'rb').read()
    try: t = b.decode('utf-8')
    except UnicodeDecodeError: t = b.decode('cp1251', errors='replace')
    t = t.replace('\r\n', '\n').replace('\r', '\n')
    return '\n'.join(l.rstrip() for l in t.split('\n'))
fj, fr = files(J), files(R)
# case-insensitive path match (Windows tree)
lj = {p.lower(): p for p in fj}; lr = {p.lower(): p for p in fr}
common = sorted(set(lj) & set(lr)); newf = sorted(set(lr) - set(lj)); gone = sorted(set(lj) - set(lr))
for side in ('a', 'b'):
    shutil.rmtree(os.path.join(W, side), ignore_errors=True)
for k in common:
    for side, root, m in (('a', J, lj), ('b', R, lr)):
        dst = os.path.join(W, side, k); os.makedirs(os.path.dirname(dst), exist_ok=True)
        open(dst, 'w', encoding='utf-8', newline='\n').write(norm(os.path.join(root, m[k])))
r = subprocess.run(['git', '-c', 'core.autocrlf=false', 'diff', '--no-index', '--numstat', 'a', 'b'], cwd=W, capture_output=True, text=True, encoding='utf-8', errors='replace')
stat = {}
for line in r.stdout.splitlines():
    a, d, path = line.split('	', 2)
    m = re.match(r'^(?:\{a => b\}|b)/(.*)$', path.replace(chr(92), '/'))
    if not m: print('unparsed', path, file=sys.stderr); continue
    stat[m.group(1).lower()] = (int(a), int(d))
def nlines(p):
    return norm(p).count('\n') + 1
def module(k):
    top = k.split('/')[0]
    if top.lower() != 'main': return top
    f = k.split('/')[-1]
    for pre, name in (('ai', 'Main/ai*'), ('aiviewer', 'Main/ai*'), ('rpg', 'Main/RPG*'), ('script', 'Main/script*'), ('sc', 'Main/script*')):
        pass
    fl = f.lower()
    if fl.startswith('script'): return 'Main/script*'
    if fl.startswith('rpg'): return 'Main/RPG*'
    if fl.startswith('ai'): return 'Main/ai*'
    if fl.startswith('event'): return 'Main/event*'
    if fl.startswith('pf') or fl.startswith('path'): return 'Main/path*'
    if re.match(r'^w[A-Z]', f): return 'Main/w* (NWorld)'
    if re.match(r'^i[A-Z]', f): return 'Main/i* (NGame UI screens)'
    if re.match(r'^(UI|ui)', f): return 'Main/UI* (NUI)'
    if re.match(r'^G[A-Z]', f): return 'Main/G* (NGScene)'
    if re.match(r'^(Gfx|gfx)', f): return 'Main/Gfx*'
    if re.match(r'^(LS|ls)', f): return 'Main/LS* (LifeStudio)'
    if re.match(r'^(Anim|anim)', f): return 'Main/Anim*'
    if re.match(r'^(Map|map)', f): return 'Main/Map*'
    if re.match(r'^(RW|rw)', f): return 'Main/RW* (save/load)'
    return 'Main/other'
agg = collections.defaultdict(lambda: [0, 0, 0, 0, 0, 0, 0, 0])  # identical, changed, new, gone, +lines, -lines, newfile lines, jan03 lines
for k in common:
    m = module(lr[k]); a, d = stat.get(k, (0, 0))
    agg[m][0 if (a, d) == (0, 0) else 1] += 1; agg[m][4] += a; agg[m][5] += d
    agg[m][7] += nlines(os.path.join(J, lj[k]))
for k in newf:
    m = module(lr[k]); agg[m][2] += 1; agg[m][6] += nlines(os.path.join(R, lr[k]))
for k in gone:
    m = module(lj[k]); agg[m][3] += 1
rows = ['%-28s %5s %5s %5s %5s %8s %8s %9s %8s' % ('module', 'same', 'chg', 'new', 'gone', '+lines', '-lines', 'newfileL', 'jan03L')]
tot = [0] * 8
for m in sorted(agg, key=lambda m: -(agg[m][4] + agg[m][6])):
    v = agg[m]; tot = [x + y for x, y in zip(tot, v)]
    rows.append('%-28s %5d %5d %5d %5d %8d %8d %9d %8d' % (m, *v))
rows.append('%-28s %5d %5d %5d %5d %8d %8d %9d %8d' % ('TOTAL', *tot))
open(OUT + '_modules.txt', 'w').write('\n'.join(rows) + '\n')
print('\n'.join(rows))
with open(OUT + '_new_files.txt', 'w') as f:
    for k in newf: f.write('%s\t%d\n' % (lr[k], nlines(os.path.join(R, lr[k]))))
with open(OUT + '_gone_files.txt', 'w') as f:
    for k in gone: f.write(lj[k] + '\n')
with open(OUT + '_changed_files.tsv', 'w') as f:
    for k in common:
        a, d = stat.get(k, (0, 0))
        if a or d: f.write('%s\t+%d\t-%d\n' % (lr[k], a, d))
print('common', len(common), 'new', len(newf), 'gone', len(gone), file=sys.stderr)
