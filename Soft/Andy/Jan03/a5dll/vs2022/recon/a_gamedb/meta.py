"""Print the descriptor chunks 5..8 (and the first record) of selected table objects."""
import sys, struct
from ss import load, chunks
from tables import table_map, objects


def leaf_repr(b):
    if len(b) == 4:
        return 'int %d' % struct.unpack('<i', b)[0]
    if len(b) >= 2 and len(b) % 2 == 0 and all(b[i + 1] == 0 for i in range(0, len(b), 2)) and all(32 <= b[i] < 127 for i in range(0, len(b), 2)):
        return 'w"%s"' % b.decode('utf-16le')
    if b and all(32 <= c < 127 for c in b):
        return '"%s"' % b.decode('latin-1')
    return b[:32].hex(' ')


def dump(d, s, n, ind, depth=0):
    for c, ps, ln in chunks(d, s, s + n):
        b = d[ps:ps + ln]
        try:
            kids = list(chunks(d, ps, ps + ln)) if ln >= 2 and depth < 4 else []
        except Exception:
            kids = []
        r = leaf_repr(b)
        if kids and not r.startswith(('w"', '"')):
            print('%s[%d] (%d)' % (ind, c, ln))
            dump(d, ps, ln, ind + '  ', depth + 1)
        else:
            print('%s[%d] %s' % (ind, c, r))


d, top = load(sys.argv[1])
ptr2id, _ = table_map(d, top)
id2ptr = {v: k for k, v in ptr2id.items()}
objs = objects(d, top)
for t in sys.argv[2:]:
    tid = int(t, 16)
    s, n = objs[id2ptr[tid]]
    print('=== table 0x%08X' % tid)
    for c, ps, ln in chunks(d, s, s + n):
        if c >= 5:
            print(' chunk', c)
            dump(d, ps, ln, '   ')
        else:
            first = next(iter(chunks(d, ps, ps + ln)), None)
            print(' chunk %d: %d items; first item:' % (c, len(list(chunks(d, ps, ps + ln)))))
            if first:
                dump(d, first[1], first[2], '   ')
