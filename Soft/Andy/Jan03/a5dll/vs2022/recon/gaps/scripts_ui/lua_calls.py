"""Lua 4.0 token scan of release scripts: called names, defined functions, globals, 5.0-only syntax.
usage: python lua_calls.py <Complete dir> <db\E000000C_Scripts.tsv> <Jan03 ScriptFunctions.cpp> <release_reglist.txt> <outdir>"""
import sys, os, re, collections, glob

KW = set('and break do else elseif end for function if in local nil not or repeat return then until while true false'.split())
TOK = re.compile(r'''(?P<cm>--[^\n]*)|(?P<ls>\[\[.*?\]\])|(?P<s>"(?:\\.|[^"\\\n])*"|'(?:\\.|[^'\\\n])*')|(?P<n>\d+\.?\d*(?:[eE][+-]?\d+)?)|(?P<id>[A-Za-z_][A-Za-z0-9_]*)|(?P<op>\.\.\.|\.\.|==|~=|<=|>=|[^\s])''', re.S)


def tokens(src):
    out = []
    for m in TOK.finditer(src):
        k = m.lastgroup
        if k == 'cm':
            out.append(('cm', m.group()))
            continue
        out.append((k, m.group()))
    return out


def analyze(name, src, acc):
    toks = tokens(src)
    code = [t for t in toks if t[0] != 'cm']
    feats = acc['feats']
    for t in toks:
        if t[0] == 'cm' and t[1].startswith('--[['):
            feats['--[[ block comment (5.0; line comment in 4.0)'][name] += 1
    locals_ = set()
    for i, (k, v) in enumerate(code):
        prev = code[i - 1][1] if i else ''
        nxt = code[i + 1] if i + 1 < len(code) else ('', '')
        if k == 'op' and v == '%' and nxt[0] == 'id' and (i == 0 or code[i - 1][0] == 'op' and code[i - 1][1] not in (')', ']', '}')):
            feats['%upvalue (4.0 only)'][name] += 1
        if k == 'op' and v == '...':
            feats['... varargs'][name] += 1
        if k != 'id':
            continue
        if v == 'local' and nxt[1] == 'function':
            feats['local function (5.0 only)'][name] += 1
        if v in ('true', 'false'):
            feats['true/false (%s)' % ('assigned: 4.0 only' if nxt[1] == '=' and code[i + 2][1] != '=' else 'used as value')][name] += 1
        if v in ('pairs', 'ipairs') and nxt[1] == '(':
            feats['pairs/ipairs (5.0 stdlib)'][name] += 1
        if v == 'in' and nxt[0] == 'id' and code[i + 2][1] not in ('(',):
            feats['for k,v in table (4.0 form)'][name] += 1
        if v in ('string', 'table', 'math', 'coroutine', 'io', 'os') and nxt[1] == '.':
            feats['%s.* lib (5.0)' % v][name] += 1
        if v in KW:
            if v == 'local':
                j = i + 1
                while j < len(code) and code[j][0] == 'id':
                    locals_.add(code[j][1])
                    if j + 1 < len(code) and code[j + 1][1] == ',':
                        j += 2
                    else:
                        break
            if v == 'for':
                j = i + 1
                while j < len(code) and code[j][0] == 'id' and code[j][1] != 'in':
                    locals_.add(code[j][1])
                    j += 2 if code[j + 1][1] == ',' else 1
                    if code[j - 1][1] in ('=',) or code[j][1] in ('=', 'in'):
                        break
            continue
        if prev in ('.', ':'):
            continue
        if prev == 'function':
            # definition: function Name( or function a.b(
            if nxt[1] in ('.', ':'):
                continue
            acc['defined'][v].add(name)
            # params
            j = i + 2
            while j < len(code) and code[j][1] != ')':
                if code[j][0] == 'id':
                    locals_.add(code[j][1])
                j += 1
            continue
        if nxt[1] == '(' or nxt[0] in ('s', 'ls') or nxt[1] == '{':
            if v in locals_:
                continue
            acc['calls'][v] += 1
            acc['callsrc'][v].add(name)
        else:
            if v in locals_:
                continue
            if nxt[1] == '=' and (i + 2 < len(code) and code[i + 2][1] != '=') and prev not in ('=', ',', '(', '..', 'return', 'and', 'or', 'not', '==', '~=', '<', '>', '<=', '>=', '+', '-', '*', '/', '[', '{'):
                acc['assigned'][v].add(name)
                if code[i + 2][1] == 'function':
                    acc['defined'][v].add(name)
            else:
                acc['refs'][v] += 1
                acc['refsrc'][v].add(name)


def main():
    comp, scripts_tsv, jan_cpp, rel_reg, out = sys.argv[1:6]
    os.makedirs(out, exist_ok=True)
    srcs = []
    for f in sorted(glob.glob(os.path.join(comp, 'Scripts', '*.l'))):
        srcs.append(('file:' + os.path.basename(f), open(f, encoding='cp1251').read()))
    used = set()
    lines = open(scripts_tsv, encoding='utf-8').read().split('\n')
    for ln in lines[1:]:
        if not ln:
            continue
        p = ln.split('\t')
        code = p[2].replace('\\r', '\r').replace('\\n', '\n').replace('\\t', '\t').replace('\\\\', '\\')
        srcs.append(('db:%s:%s' % (p[0], p[1]), code))
    acc = {'calls': collections.Counter(), 'callsrc': collections.defaultdict(set), 'defined': collections.defaultdict(set),
           'assigned': collections.defaultdict(set), 'refs': collections.Counter(), 'refsrc': collections.defaultdict(set),
           'feats': collections.defaultdict(collections.Counter)}
    for n, s in srcs:
        analyze(n, s, acc)
    # Jan03 registered
    txt = open(jan_cpp, encoding='cp1251').read()
    body = txt[txt.index('pRegList[] ='):]
    body = body[:body.index('};')]
    body = re.sub(r'//[^\n]*', '', body)
    jan = set(re.findall(r'\{\s*"(\w+)"', body)) | set(re.findall(r'REG_FUNCTION\(\s*(\w+)\s*\)', body))
    rel = set(l.split('\t')[0] for l in open(rel_reg, encoding='utf-8-sig') if l.strip() and not l.startswith('#'))
    defined = set(acc['defined'])
    with open(os.path.join(out, 'jan03_reglist.txt'), 'w') as f:
        f.write('\n'.join(sorted(jan)) + '\n')
    calls = acc['calls']
    rows = []
    for nm, c in calls.most_common():
        st = 'lua-defined' if nm in defined else ('jan03' if nm in jan else ('RELEASE-ONLY-C' if nm in rel else 'UNKNOWN'))
        rows.append((nm, c, len(acc['callsrc'][nm]), st, nm in jan, nm in rel, nm in defined))
    with open(os.path.join(out, 'script_calls.tsv'), 'w') as f:
        f.write('name\tcallsites\tnsources\tstatus\tin_jan03\tin_release_reg\tlua_defined\n')
        for r in rows:
            f.write('\t'.join(map(str, r)) + '\n')
    # globals referenced but not called (variables / callbacks) and not assigned anywhere
    with open(os.path.join(out, 'script_globals_unassigned.tsv'), 'w') as f:
        for nm, c in acc['refs'].most_common():
            if nm not in acc['assigned'] and nm not in defined and nm not in jan and nm not in rel:
                f.write('%s\t%d\t%s\n' % (nm, c, ','.join(sorted(acc['refsrc'][nm]))[:200]))
    with open(os.path.join(out, 'script_syntax.txt'), 'w') as f:
        for k, v in sorted(acc['feats'].items()):
            f.write('%s: %d occurrences in %d sources: %s\n' % (k, sum(v.values()), len(v), ', '.join(sorted(v))[:400]))
    cs = [r for r in rows if r[3] != 'lua-defined']
    miss = [r for r in rows if r[3] in ('RELEASE-ONLY-C', 'UNKNOWN')]
    print('sources:', len(srcs), 'db scripts:', sum(1 for n, _ in srcs if n.startswith('db:')))
    print('distinct called names:', len(rows), 'callsites:', sum(calls.values()))
    print('  lua-defined:', sum(1 for r in rows if r[3] == 'lua-defined'))
    print('  C-called (not lua-defined):', len(cs), ' in Jan03:', sum(1 for r in cs if r[4]), ' missing:', len(miss),
          ' (release-reg:', sum(1 for r in miss if r[3] == 'RELEASE-ONLY-C'), 'unknown:', sum(1 for r in miss if r[3] == 'UNKNOWN'), ')')
    print('  missing callsites:', sum(r[1] for r in miss), 'of', sum(r[1] for r in cs))
    print('Jan03 reg:', len(jan), 'release reg:', len(rel), 'common:', len(jan & rel), 'jan-only:', sorted(jan - rel), 'release-only count:', len(rel - jan))
    with open(os.path.join(out, 'reglist_diff.txt'), 'w', encoding='utf-8') as f:
        f.write('Jan03 %d, release %d, common %d\n' % (len(jan), len(rel), len(jan & rel)))
        f.write('Jan03-only: %s\n' % ' '.join(sorted(jan - rel)))
        f.write('release-only (%d): %s\n' % (len(rel - jan), ' '.join(sorted(rel - jan))))
    print('top missing:')
    for r in miss[:40]:
        print('  %-28s %4d sites %3d srcs %s' % (r[0], r[1], r[2], r[3]))


if __name__ == '__main__':
    main()
