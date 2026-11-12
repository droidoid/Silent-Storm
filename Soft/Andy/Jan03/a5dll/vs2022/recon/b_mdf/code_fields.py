# Extract tables/fields the Jan03 DBFormat code expects. Usage: python -I code_fields.py <a5dll dir> <out.json>
import sys, os, re, json, collections

root = sys.argv[1]; out = sys.argv[2]
src = {}
for d in ('DBFormat',):
    for fn in os.listdir(os.path.join(root, d)):
        if fn.lower().endswith(('.cpp', '.h')):
            with open(os.path.join(root, d, fn), 'rb') as f:
                src[d + '/' + fn] = f.read().decode('cp1251', errors='replace')


def strip_comments(t):
    out = []; i = 0; n = len(t)
    while i < n:
        c = t[i]
        if c == '"':
            j = i + 1
            while j < n and t[j] != '"':
                j += 2 if t[j] == '\\' else 1
            out.append(t[i:j + 1]); i = j + 1
        elif t.startswith('//', i):
            j = t.find('\n', i); i = n if j < 0 else j
        elif t.startswith('/*', i):
            j = t.find('*/', i + 2); i = n if j < 0 else j + 2
        else:
            out.append(c); i += 1
    return ''.join(out)


def body_at(t, pos):
    i = t.find('{', pos); depth = 0
    for j in range(i, len(t)):
        if t[j] == '{': depth += 1
        elif t[j] == '}':
            depth -= 1
            if depth == 0:
                return t[i:j + 1], j + 1
    return t[i:], len(t)


# disable code in #if 0 blocks
def strip_if0(t):
    return re.sub(r'#if\s+0\b.*?#endif', '', t, flags=re.S)


helpers = {}
for fn, t in list(src.items()):
    t2 = strip_if0(strip_comments(t))
    for m in re.finditer(r'(?:static\s+)?void\s+(\w+)\s*\([^;{)]*\)\s*(?=\{)', t2):
        if m.group(1) in ('Import', 'ImportField', 'ImportRelation'):
            continue
        body, end = body_at(t2, m.end())
        fl = re.findall(r'ImportField\s*\(\s*"([^"]+)"', body)
        if fl:
            helpers[m.group(1)] = fl
print('helper functions with ImportField:', {k: len(v) for k, v in helpers.items()})

regs = []; rels = []
imports = {}       # class -> dict(fields=[], calls=[], file=)
bases = {}
spans = collections.defaultdict(list)
for fn, t in src.items():
    t = strip_if0(strip_comments(t))
    src[fn] = t
    for m in re.finditer(r'REGISTER_DATABASE_CLASS\s*\(\s*(\w+)\s*,\s*"([^"]+)"\s*,\s*(\w+)\s*\)', t):
        regs.append(dict(n=m.group(1), table=m.group(2), cls=m.group(3), file=fn))
    for m in re.finditer(r'REGISTER_DATABASE_CLASS_TEMPL\s*\(\s*(\w+)\s*,\s*"([^"]+)"\s*,\s*(\w+)\s*,\s*(\w+)\s*\)', t):
        regs.append(dict(n=m.group(1), table=m.group(2), cls=m.group(3), templ=m.group(4), file=fn))
    for m in re.finditer(r'REGISTER_DATABASE_RELATION\s*\(\s*"([^"]+)"\s*\)', t):
        rels.append(m.group(1))
    for m in re.finditer(r'(?:class|struct)\s+(\w+)\s*(?:<[^>{;]*>)?\s*:\s*(?:public|protected|private)?\s*(\w+)', t):
        bases.setdefault(m.group(1), m.group(2))
    # typedef CRPGSomethingForPers<CRPGWeapon> CRPGWeapon4Pers;
    for m in re.finditer(r'typedef\s+(\w+)\s*<[^;]*>\s*(\w+)\s*;', t):
        bases.setdefault(m.group(2), m.group(1))
    for m in re.finditer(r'(?:void|virtual\s+void)\s+(?:(\w+)\s*(?:<[^>]*>)?\s*::\s*)?Import\s*\(\s*\)\s*(?=\{)', t):
        cls = m.group(1)
        if cls is None:
            # inline definition inside class body: find enclosing class name
            k = t.rfind('class ', 0, m.start()); km = re.match(r'class\s+(\w+)', t[k:]) if k >= 0 else None
            cls = km.group(1) if km else '?'
        body, end = body_at(t, m.end())
        spans[fn].append((m.start(), end))
        flds = re.findall(r'ImportField\s*\(\s*"([^"]+)"', body)
        dyn = []
        if 'pF->pszFlag' in body:
            for arrname in ('animPWFlags', 'animCSFlags'):
                ta = re.search(arrname + r'\[\]\s*=\s*\{(.*?)\};', body, re.S)
                if ta:
                    dyn += re.findall(r'"([^"]+)"', ta.group(1))
        if '"Texture%d"' in body: dyn += ['Texture%d' % i for i in range(48)]
        if 'CampZone%d' in body: dyn += ['CampZone%d' % i for i in range(1, 5)]
        if 'string( "Diplomacy" )' in body: dyn += ['Diplomacy%d' % i for i in range(1, 17)]
        if 'AllowWeaponType%d' in body: dyn += ['AllowWeaponType%d' % i for i in range(1, 8)]
        if 'pszUniformPlaces' in body:
            for p in ['BeltL1', 'BeltR1', 'BeltM1', 'BeltMediumL1', 'BeltMediumR1', 'BeltMediumL2', 'BeltMediumR2', 'WaistBeltL1', 'WaistBeltR1']:
                dyn += ['ST' + p, 'Model' + p]
        for h, hf in helpers.items():
            if re.search(r'\b' + h + r'\s*(?:<[^>]*>)?\s*\(', body):
                flds = flds + hf
        calls = [c for c in re.findall(r'(\w+)\s*(?:<[^>]*>)?\s*::\s*Import\s*\(\s*\)', body) if c != cls]
        rel = 'ImportRelation' in body
        e = imports.setdefault(cls, dict(fields=[], dyn=[], calls=[], file=fn, relation=False))
        e['fields'] += flds; e['dyn'] += dyn; e['calls'] += calls; e['relation'] |= rel

# ImportField calls outside any Import() body
stray = []
for fn, t in src.items():
    for m in re.finditer(r'ImportField\s*\(\s*([^,]+),', t):
        if not any(a <= m.start() < b for a, b in spans[fn]):
            if fn.endswith('.h') and 'void ImportField' in t[max(0, m.start() - 30):m.start() + 5]:
                continue
            stray.append((fn, t.count('\n', 0, m.start()) + 1, m.group(1).strip()))


def resolve(cls, seen=None):
    seen = seen or set()
    if cls in seen: return [], []
    seen.add(cls)
    if cls in imports:
        e = imports[cls]; f = list(e['fields']); dn = list(e['dyn'])
        for c in e['calls']:
            a, b = resolve(c, seen); f += a; dn += b
        return f, dn
    if cls in bases:
        return resolve(bases[cls], seen)
    return [], []


tables = {}
for r in regs:
    c = r.get('templ') or r['cls']
    # for TEMPL registration the record class is `name` (r['cls']) built from template className
    f, dn = resolve(r['cls'])
    if not f and not dn and r.get('templ'):
        f, dn = resolve(r['templ'])
    allf = []
    for x in ['ID'] + f + dn:
        if x not in allf: allf.append(x)
    tables[r['table']] = dict(cls=r['cls'], n=r['n'], fields=allf, dynamic=sorted(set(dn)), file=r['file'])
res = dict(tables=tables, relations=rels, stray_importfield=stray, unresolved=[r['cls'] for r in regs if len(tables[r['table']]['fields']) <= 1])
with open(out, 'w') as f:
    json.dump(res, f, indent=1)
print('registered tables:', len(tables), 'relations:', rels)
print('tables with no fields resolved:', res['unresolved'])
print('stray ImportField (outside Import bodies):', len(stray))
for s in stray[:40]: print('  ', s)
