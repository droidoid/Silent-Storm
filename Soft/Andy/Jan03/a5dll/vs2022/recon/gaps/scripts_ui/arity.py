"""Argument counts at release call sites vs Jan03 luaPrepareData format strings (common C functions).
usage: python arity.py <Complete dir> <db\E000000C_Scripts.tsv> <Jan03 Main dir> > arity.txt"""
import sys, os, re, glob, collections
sys.path.insert(0, os.path.dirname(__file__))
from lua_calls import tokens  # noqa (lua_calls.main runs only via __main__ guard below)

comp, tsv, main = sys.argv[1:4]
fmt = collections.defaultdict(set)
for fn in glob.glob(os.path.join(main, 'script*.cpp')):
    src = open(fn, encoding='cp1251', errors='replace').read()
    for n1, n2, f in re.findall(r'(?:luaPrepareData\(\s*\w+\s*,\s*"(\w+)"|BEGIN_SCRIPT_COMMAND\(\s*(\w+))\s*,\s*"([^"]*)"', src):
        name = n1 or n2
        f2 = re.sub(r'\[[^\]]*\]', '', f)
        req = len(re.sub(r'.\[[^\]]*\]', '', f).replace('.', ''))
        fmt[name].add((f, req, len(f2.replace('.', '')), '.' in f))
srcs = [open(f, encoding='cp1251').read() for f in glob.glob(os.path.join(comp, 'Scripts', '*.l'))]
for ln in open(tsv, encoding='utf-8').read().split('\n')[1:]:
    if ln:
        srcs.append(ln.split('\t')[2].replace('\\r', '\r').replace('\\n', '\n').replace('\\t', '\t').replace('\\\\', '\\'))
argc = collections.defaultdict(collections.Counter)
for s in srcs:
    code = [t for t in tokens(s) if t[0] != 'cm']
    for i in range(len(code) - 1):
        if code[i][0] == 'id' and code[i + 1][1] == '(' and code[i][1] in fmt and (i == 0 or code[i - 1][1] not in ('.', ':', 'function')):
            depth, n, j, empty = 0, 1, i + 1, True
            while j < len(code):
                v = code[j][1]
                if v in ('(', '{', '['):
                    depth += 1
                elif v in (')', '}', ']'):
                    depth -= 1
                    if depth == 0:
                        break
                elif v == ',' and depth == 1:
                    n += 1
                if j > i + 1:
                    empty = False
                j += 1
            argc[code[i][1]][0 if empty else n] += 1
few = more = 0
for name in sorted(argc):
    reqs = min(x[1] for x in fmt[name])
    mx = max(x[2] for x in fmt[name])
    var = any(x[3] for x in fmt[name])
    lo = sum(c for k, c in argc[name].items() if k < reqs)
    hi = 0 if var else sum(c for k, c in argc[name].items() if k > mx)
    few += lo
    more += hi
    if lo or hi:
        print('%-24s jan03 fmt=%s  release argc=%s  too_few=%d too_many=%d' % (name, sorted(x[0] for x in fmt[name]), dict(argc[name]), lo, hi))
print('TOTAL call sites of common functions with Jan03 format: %d; too few args (runtime error in Jan03): %d; extra args (ignored): %d' % (
    sum(sum(c.values()) for c in argc.values()), few, more))

