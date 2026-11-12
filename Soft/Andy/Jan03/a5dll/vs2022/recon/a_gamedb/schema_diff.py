"""Compare the table schema expected by Jan03 DBFormat (REGISTER_DATABASE_* + Class::Import() field names)
with the column descriptors stored in a release game.db (class 0xA1843130 tables).

usage: python schema_diff.py <a5dll dir> <game.db> <out dir>
"""
import os, re, sys, struct, collections
from ss import load, chunks
from tables import table_map, objects, first

SRC, DB, OUT = sys.argv[1], sys.argv[2], sys.argv[3]

# ---------------- Jan03 side ----------------
text = {}
for sub in ('DBFormat', 'Main', 'ADOImport'):
    for root, _, files in os.walk(os.path.join(SRC, sub)):
        for f in files:
            if f.lower().endswith(('.cpp', '.h')):
                p = os.path.join(root, f)
                text[p] = open(p, 'rb').read().decode('cp1251', 'replace')
alltext = '\n'.join(text.values())

reg = re.compile(r'REGISTER_DATABASE_CLASS(?:_TEMPL)?\s*\(\s*([0-9xXa-fA-F]+)\s*,\s*"([^"]+)"\s*,\s*(\w+)\s*(?:,\s*(\w+)\s*)?\)')
rel = re.compile(r'REGISTER_DATABASE_RELATION\s*\(\s*"([^"]+)"\s*\)')
dataformat = text[[p for p in text if p.endswith(os.path.join('DBFormat', 'DataFormat.cpp'))][0]]
jan_tables = []   # (id, name, class)
for m in reg.finditer(dataformat):
    jan_tables.append((int(m.group(1), 0) & 0xFFFFFFFF, m.group(2), m.group(3)))
jan_rel = rel.findall(dataformat)


def import_body(cls):
    """body of cls::Import() (non-template or template definition)"""
    m = re.search(r'void\s+' + re.escape(cls) + r'(?:<[^>]*>)?\s*::\s*Import\s*\(\s*\)\s*\{', alltext)
    if not m:
        # inline in class definition
        m2 = re.search(r'class\s+' + re.escape(cls) + r'\b[^;{]*\{', alltext)
        if m2:
            body = block(alltext, m2.end() - 1)
            m3 = re.search(r'void\s+Import\s*\(\s*\)\s*\{', body)
            if m3:
                return block(body, m3.end() - 1)
        return None
    return block(alltext, m.end() - 1)


def block(s, i):
    depth = 0
    for j in range(i, len(s)):
        if s[j] == '{':
            depth += 1
        elif s[j] == '}':
            depth -= 1
            if depth == 0:
                return s[i + 1:j]
    return s[i + 1:]


def strip_comments(s):
    s = re.sub(r'/\*.*?\*/', '', s, flags=re.S)
    return re.sub(r'//[^\n]*', '', s)


FIELD = re.compile(r'ImportField\s*\(\s*"([^"]+)"')
DYN = re.compile(r'ImportField\s*\(\s*[^"\s]')
PARENT = re.compile(r'(\w+)\s*::\s*Import\s*\(\s*\)\s*;')


def fields_of(cls, seen=None):
    seen = seen or set()
    if cls in seen:
        return set(), 0, False
    seen.add(cls)
    body = import_body(cls)
    if body is None:
        return set(), 0, False
    body = strip_comments(body)
    names = set(FIELD.findall(body))
    dyn = len(DYN.findall(body))
    for p in PARENT.findall(body):
        if p != cls:
            n2, d2, _ = fields_of(p, seen)
            names |= n2
            dyn += d2
    return names, dyn, True


# ---------------- release side ----------------
d, top = load(DB)
ptr2id, keys = table_map(d, top)
objs = objects(d, top)
TYPE = {0: 'int', 2: 'float', 3: 'string'}
rel_tables = {}
for ptr, tid in ptr2id.items():
    s, n = objs[ptr]
    cols, rows = [], 0
    for c, ps, ln in chunks(d, s, s + n):
        if c == 5:
            for _, cs, cl in chunks(d, ps, ps + ln):
                nm = first(d, cs, cl, 2)
                ty = first(d, cs, cl, 3)
                tval = None
                if ty and ty[1] == 4:
                    tval = struct.unpack_from('<i', d, ty[0])[0]
                cols.append((d[nm[0]:nm[0] + nm[1]].decode('cp1251'), TYPE.get(tval, str(tval))))
        elif c == 2:
            rows = sum(1 for _ in chunks(d, ps, ps + ln))
    rel_tables[tid] = (cols, rows)
# relations stored in root chunk 1 -> [2]
root = [x for x in top if x[0] == 1][0]
r2 = first(d, root[1], root[2], 2)
rel_names = []
if r2:
    for _, ps, ln in chunks(d, r2[0], r2[0] + r2[1]):
        nm = first(d, ps, ln, 2)
        if nm:
            rel_names.append(d[nm[0]:nm[0] + nm[1]].decode('cp1251'))

# ---------------- compare ----------------
os.makedirs(OUT, exist_ok=True)
jan_ids = {t[0]: t for t in jan_tables}
common = sorted(set(jan_ids) & set(rel_tables))
only_jan = sorted(set(jan_ids) - set(rel_tables))
only_rel = sorted(set(rel_tables) - set(jan_ids))
lines = []
stat = collections.Counter()
missing_total = 0
for tid in common:
    _, name, cls = jan_ids[tid]
    need, dyn, found = fields_of(cls)
    cols, rows = rel_tables[tid]
    have = {c for c, _ in cols}
    have_l = {c.lower() for c in have}
    missing = sorted(f for f in need if f.lower() not in have_l)
    case_only = sorted(f for f in need if f not in have and f.lower() in have_l)
    unused = sorted(c for c in have if c.lower() not in {f.lower() for f in need} and c != 'ID')
    if not found:
        stat['import() not found'] += 1
    elif missing:
        stat['missing fields'] += 1
    else:
        stat['all fields present'] += 1
    missing_total += len(missing)
    lines.append('0x%08X %-28s %-26s rows=%-6d cols=%-3d need=%-3d dyn=%d missing=%d unused=%d %s' % (
        tid, name, cls, rows, len(cols), len(need), dyn, len(missing), len(unused), '' if found else 'IMPORT-NOT-FOUND'))
    if missing:
        lines.append('      missing in release: ' + ', '.join(missing))
    if case_only:
        lines.append('      differs only in case: ' + ', '.join(case_only))
    if unused:
        lines.append('      release-only columns: ' + ', '.join(unused))

with open(os.path.join(OUT, 'schema_diff.txt'), 'w', encoding='utf-8') as f:
    f.write('Jan03 tables: %d (+%d relations), release tables: %d (+%d relations: %s)\n' % (
        len(jan_tables), len(jan_rel), len(rel_tables), len(rel_names), ', '.join(rel_names)))
    f.write('Jan03 relations: %s\n' % ', '.join(jan_rel))
    f.write('common table ids: %d; only Jan03: %d; only release: %d\n' % (len(common), len(only_jan), len(only_rel)))
    f.write('per common table: %s; total missing field names: %d\n\n' % (dict(stat), missing_total))
    f.write('\n'.join(lines))
    f.write('\n\nonly in Jan03:\n')
    for tid in only_jan:
        f.write('  0x%08X %s %s\n' % (tid, jan_ids[tid][1], jan_ids[tid][2]))
    f.write('\nonly in release (id, rows, first columns):\n')
    for tid in only_rel:
        cols, rows = rel_tables[tid]
        f.write('  0x%08X rows=%d cols=%d: %s\n' % (tid, rows, len(cols), ', '.join(c for c, _ in cols[:12])))
with open(os.path.join(OUT, 'release_columns.txt'), 'w', encoding='utf-8') as f:
    for tid in sorted(rel_tables):
        cols, rows = rel_tables[tid]
        nm = jan_ids[tid][1] if tid in jan_ids else '?'
        f.write('0x%08X %s rows=%d\n    %s\n' % (tid, nm, rows, ', '.join('%s:%s' % c for c in cols)))
print(open(os.path.join(OUT, 'schema_diff.txt'), encoding='utf-8').read()[:600])
