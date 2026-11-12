import sys, math, re, collections
from pdb import sections

for exe in sys.argv[1:]:
    d, base, secs = sections(exe)
    print('==', exe)
    for sn, va, vs, rp, rs in secs:
        raw = d[rp:rp + rs]
        c = collections.Counter(raw)
        ent = -sum(v / len(raw) * math.log2(v / len(raw)) for v in c.values()) if raw else 0
        print('  %-8s va=%08X vsize=%-8d raw=%-8d entropy=%.2f' % (sn, va, vs, rs, ent))
    rtti = set(re.findall(rb'\.\?A[VU][A-Za-z0-9_@$?]+@@', d))
    print('  RTTI type names: %d; NDb: %d' % (len(rtti), sum(1 for r in rtti if b'@NDb@@' in r)))
    for key in (b'CDBTableDataStorage', b'IOLETable', b'CDBTableBase', b'SColumnInfo', b'NDatabase'):
        print('   %-20s %s' % (key.decode(), [m.decode() for m in sorted(rtti) if key in m][:3]))
    ascii_strings = re.findall(rb'[\x20-\x7e]{6,}', d)
    print('  printable strings >=6: %d' % len(ascii_strings))
    for pat in (rb'\.cpp', rb'game\.db', rb'A5GAME', rb'DRIVER=', rb'\.pdb'):
        hits = sorted(set(s for s in ascii_strings if re.search(pat, s, re.I)))
        print('   /%s/ %d e.g. %s' % (pat.decode(), len(hits), [h.decode()[:70] for h in hits[:4]]))
    imp = sorted(set(re.findall(rb'[A-Za-z0-9_]+\.(?:dll|DLL)', d)))
    print('  dll names:', [i.decode() for i in imp][:40])
