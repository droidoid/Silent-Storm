"""Jan03 interface code (Main\*.cpp) vs release UI data.
- literal NDb::Get<X>(N) ids -> exist in release table?
- GetControl("id") per source file -> exists among controls of containers loaded in that file (incl. nested), else anywhere?
usage: python ui_check.py <Jan03 Main dir> <db dir> > ui_check.txt"""
import sys, os, re, csv, glob, collections
csv.field_size_limit(10 ** 9)
main, dbd = sys.argv[1], sys.argv[2]


def load(prefix):
    f = glob.glob(os.path.join(dbd, prefix + '_*.tsv'))[0]
    return list(csv.DictReader(open(f, encoding='utf-8'), delimiter='\t', quoting=csv.QUOTE_NONE))


ctrls = load('0000002B')
conts = {int(r['ID']): r for r in load('0000002C')}
TABLES = {'UITexture': '00000031', 'UIContainer': '0000002C', 'String': '0000002D', 'Sound': '00000025', 'Texture': '00000003',
          'Music': '0000004C', 'Model': '00000001', 'AISound': '00000034', 'TAmbientLight': '0000005C', 'TEffect': '00000042',
          'Effect': '00000041', 'Material': '00000009', 'Pers': 'E0000004', 'Armor': 'E0000005', 'RPGGrenade': 'E000000D'}
ids = {k: set(int(r['ID']) for r in load(v)) for k, v in TABLES.items()}
by_cont = collections.defaultdict(list)
for c in ctrls:
    by_cont[int(c['UIContainerID'])].append(c)
all_ids = collections.Counter(c['IDText'].lower() for c in ctrls)


def names_in(cid, seen=None):
    seen = seen or set()
    if cid in seen:
        return set()
    seen.add(cid)
    s = set()
    for c in by_cont.get(cid, []):
        s.add(c['IDText'].lower())
        n = int(c['NestedUIContainerID'] or 0)
        if n:
            s |= names_in(n, seen)
    return s


lit = collections.defaultdict(lambda: [0, 0, set()])
ctl_tot = ctl_ok_cont = ctl_ok_any = 0
miss_ctl = []
cont_used = set()
for fn in sorted(glob.glob(os.path.join(main, '*.cpp'))):
    src = open(fn, encoding='cp1251', errors='replace').read()
    src = re.sub(r'//[^\n]*', '', src)
    for t, n in re.findall(r'NDb::Get(\w+)\(\s*(\d+)\s*\)', src):
        n = int(n)
        if t in ids:
            e = lit[t]
            e[0] += 1
            if n in ids[t]:
                e[1] += 1
            else:
                e[2].add(n)
    cs = [int(x) for x in re.findall(r'GetUIContainer\(\s*(\d+)\s*\)', src)]
    cont_used |= set(cs)
    avail = set()
    for c in cs:
        avail |= names_in(c)
    for nm in re.findall(r'GetControl\(\s*"([^"]+)"\s*\)', src):
        ctl_tot += 1
        if nm.lower() in avail:
            ctl_ok_cont += 1
        elif nm.lower() in all_ids:
            ctl_ok_any += 1
            if cs:
                miss_ctl.append((os.path.basename(fn), nm + ' (elsewhere only)', cs))
        else:
            miss_ctl.append((os.path.basename(fn), nm, cs))
print('== literal DB ids in Jan03 Main code (NDb::Get<T>(N)) vs release tables')
for t, (tot, ok, miss) in sorted(lit.items()):
    print('%-14s refs=%4d found=%4d missing ids=%s' % (t, tot, ok, sorted(miss)))
print('\n== UI containers loaded by Jan03 code: %d distinct' % len(cont_used))
for c in sorted(cont_used):
    r = conts.get(c)
    print('  %4d %s' % (c, ('%s %sx%s controls=%d' % (r['UserName'], r['Width'], r['Height'], len(by_cont.get(c, [])))) if r else 'MISSING'))
print('\n== GetControl("id") calls: %d; found in the file\'s containers: %d; found elsewhere only: %d; not found: %d' % (ctl_tot, ctl_ok_cont, ctl_ok_any, len(miss_ctl)))
for f, n, cs in miss_ctl:
    print('  MISSING %-22s %-28s containers=%s' % (f, n, cs))
used_cont_ids = set()
for c in cont_used:
    used_cont_ids |= {c}
print('\n== release containers: %d; loaded directly by Jan03 code: %d' % (len(conts), len(cont_used & set(conts))))
print('release control type histogram:', collections.Counter(int(c['Type']) for c in ctrls).most_common())
print('release controls with ToolTipAnchor/Freeze/Text set: anchor=%d freeze=%d text=%d' % (
    sum(1 for c in ctrls if c['ToolTipAnchorType'] not in ('', 'None')), sum(1 for c in ctrls if c['Freeze'] not in ('0', '')),
    sum(1 for c in ctrls if c['Text'])))
print('release UIContainers with ScriptID != 0:', sum(1 for r in conts.values() if r['ScriptID'] not in ('0', '')))

