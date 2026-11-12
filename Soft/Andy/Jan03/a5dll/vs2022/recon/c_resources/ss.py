"""Mini re-implementation of the Jan03 CStructureSaver READ path (FileIO/BasicChunk1.{h,cpp}) for static checks.

File = top-level chunks: 0 = object table (n x {typeID:4, serverPtr:4, valid:1}), 2 = object data, 1 = main data.
Schemas mirror what the Jan03 operator& / Add() calls request:
  ('pod', n)        DataChunk: memcpy n bytes (Jan03 only ASSERTs the length; release build copies n regardless)
  ('str',)          string: whole chunk
  ('dvec', n)       vector of POD: sub 1 = int count, sub 2 = count*n bytes            (DoDataVector)
  ('vec', S)        vector/list of serializable: one sub-chunk 1 per element          (DoVector)
  ('hmap', K, V)    hash_map: sub-chunks 1 = keys, 2 = values (i-th with i-th)        (DoHashMap)
  ('a2d', n)        CArray2D of POD: 1 = x, 2 = y, 3 = x*y*n bytes                      (Do2DArrayData)
  ('a2dobj', S)     CArray2D of serializable: 1 = x, 2 = y, x*y sub-chunks 3             (Do2DArray)
  ('struct', {id: S, ...}, name)
"""
import struct

def parse(buf):
    pos, out = 0, []
    while pos < len(buf):
        if pos + 2 > len(buf):
            return None
        cid = buf[pos]
        b = buf[pos + 1]
        if b & 1:
            if pos + 5 > len(buf):
                return None
            ln = int.from_bytes(buf[pos + 1:pos + 5], 'little') >> 1
            h = 5
        else:
            ln = b >> 1
            h = 2
        if pos + h + ln > len(buf):
            return None
        out.append((cid, buf[pos + h:pos + h + ln]))
        pos += h + ln
    return out

def parse_lenient(buf):
    """As ReadShortChunk: stop at the first chunk that does not fit; return (chunks, bytes_consumed)."""
    pos, out = 0, []
    while pos + 2 <= len(buf):
        cid = buf[pos]
        b = buf[pos + 1]
        if b & 1:
            if pos + 5 > len(buf):
                break
            ln = int.from_bytes(buf[pos + 1:pos + 5], 'little') >> 1
            h = 5
        else:
            ln = b >> 1
            h = 2
        if pos + h + ln > len(buf):
            break
        out.append((cid, buf[pos + h:pos + h + ln]))
        pos += h + ln
    return out, pos

def top(data):
    ch, used = parse_lenient(data)
    return ch, used

def top_main(data):
    ch, _ = parse_lenient(data)
    for c, p in ch:
        if c == 1:
            return parse_lenient(p)[0]
    return []

def object_table(data):
    ch, _ = parse_lenient(data)
    objs = []
    for c, p in ch:
        if c == 0:
            for i in range(0, len(p) - 8, 9):
                tid, ptr, valid = struct.unpack_from('<IIB', p, i)
                objs.append((tid, ptr, valid))
    return objs

def hashmap_items(p):
    ch = parse_lenient(p)[0]
    keys = [x for c, x in ch if c == 1]
    vals = [x for c, x in ch if c == 2]
    return list(zip(keys, vals))

def nth(chunks, cid, n=1):
    k = 0
    for c, p in chunks:
        if c == cid:
            k += 1
            if k == n:
                return p
    return None

class Report:
    def __init__(self):
        self.missing = {}    # path -> count
        self.unknown = {}    # path -> count
        self.sizebad = {}    # path -> list of (expected, got)
        self.counts = {}     # path -> list of numbers
        self.notes = []
    def add(self, d, k, v=1):
        d[k] = d.get(k, 0) + v

def i32(b):
    return struct.unpack('<i', b[:4])[0] if len(b) >= 4 else None

def check(payload, schema, path, rep):
    """payload = bytes of the chunk holding this value. Returns a python summary value (small)."""
    kind = schema[0]
    if kind == 'pod':
        if len(payload) != schema[1]:
            rep.sizebad.setdefault(path, []).append((schema[1], len(payload)))
        return payload
    if kind == 'str':
        return payload
    ch, used = parse_lenient(payload)
    if used != len(payload):
        rep.notes.append('%s: %d trailing bytes not parseable as chunks' % (path, len(payload) - used))
    if kind == 'dvec':
        n = nth(ch, 1)
        cnt = i32(n) if n is not None else 0
        if n is None:
            rep.add(rep.missing, path + '/1(count)')
        elif len(n) != 4:
            rep.sizebad.setdefault(path + '/1', []).append((4, len(n)))
        d = nth(ch, 2)
        exp = (cnt or 0) * schema[1]
        if cnt and d is None:
            rep.add(rep.missing, path + '/2(data)')
        elif cnt and len(d) != exp:
            rep.sizebad.setdefault(path + '/2', []).append(('%d*%d=%d' % (cnt, schema[1], exp), len(d)))
        for c, _ in ch:
            if c not in (1, 2):
                rep.add(rep.unknown, path + '/' + str(c))
        n1 = sum(1 for c, _ in ch if c == 1); n2 = sum(1 for c, _ in ch if c == 2)
        if n1 > 1 or n2 > 1 or (not cnt and d):
            # not a DoDataVector layout (e.g. a hash_map / DoVector written where Jan03 expects a POD vector)
            rep.sizebad.setdefault(path + '(layout)', []).append(('1x count + 1x data', '%d x id1, %d x id2, count=%s' % (n1, n2, cnt)))
        rep.counts.setdefault(path, []).append(cnt)
        return cnt
    if kind == 'vec':
        els = [p for c, p in ch if c == 1]
        for c, _ in ch:
            if c != 1:
                rep.add(rep.unknown, path + '/' + str(c))
        for p in els:
            check(p, schema[1], path + '[]', rep)
        rep.counts.setdefault(path, []).append(len(els))
        return len(els)
    if kind == 'hmap':
        ks = [p for c, p in ch if c == 1]
        vs = [p for c, p in ch if c == 2]
        for c, _ in ch:
            if c not in (1, 2):
                rep.add(rep.unknown, path + '/' + str(c))
        if len(ks) != len(vs):
            rep.notes.append('%s: %d keys vs %d values' % (path, len(ks), len(vs)))
        for k in ks:
            check(k, schema[1], path + '{key}', rep)
        for v in vs:
            check(v, schema[2], path + '{val}', rep)
        rep.counts.setdefault(path, []).append(len(ks))
        return len(ks)
    if kind in ('a2d', 'a2dobj'):
        x = nth(ch, 1); y = nth(ch, 2)
        nx = i32(x) if x else 0; ny = i32(y) if y else 0
        if x is None: rep.add(rep.missing, path + '/1(x)')
        if y is None: rep.add(rep.missing, path + '/2(y)')
        if kind == 'a2d':
            d = nth(ch, 3)
            exp = (nx or 0) * (ny or 0) * schema[1]
            if exp and d is None:
                rep.add(rep.missing, path + '/3(data)')
            elif exp and len(d) != exp:
                rep.sizebad.setdefault(path + '/3', []).append(('%dx%dx%d=%d' % (nx, ny, schema[1], exp), len(d)))
        else:
            els = [p for c, p in ch if c == 3]
            if len(els) != (nx or 0) * (ny or 0):
                rep.notes.append('%s: %d elements for %dx%d' % (path, len(els), nx, ny))
            for p in els:
                check(p, schema[1], path + '[][]', rep)
        for c, _ in ch:
            if c not in (1, 2, 3):
                rep.add(rep.unknown, path + '/' + str(c))
        rep.counts.setdefault(path, []).append('%dx%d' % (nx or 0, ny or 0))
        return (nx, ny)
    if kind == 'struct':
        fields = schema[1]
        res = {}
        present = set(c for c, _ in ch)
        for fid, fs in fields.items():
            p = nth(ch, fid)
            if p is None:
                rep.add(rep.missing, '%s.%d' % (path, fid))
                continue
            res[fid] = check(p, fs, '%s.%d' % (path, fid), rep)
        for c in sorted(present - set(fields)):
            rep.add(rep.unknown, '%s.%d' % (path, c))
        return res
    raise ValueError(kind)

def check_file(data, main_schema, rep, name='main'):
    """main_schema: ('struct', {...}) applied to top-level chunk 1 contents. Also reports top-level layout."""
    ch, used = parse_lenient(data)
    if used != len(data):
        rep.notes.append('top level: only %d of %d bytes parse as chunks' % (used, len(data)))
    ids = [c for c, _ in ch]
    main = nth(ch, 1)
    if main is None:
        rep.add(rep.missing, 'top.1(main data)')
        return None
    for c in ids:
        if c not in (0, 1, 2):
            rep.add(rep.unknown, 'top.%d' % c)
    return check(main, main_schema, name, rep)

def summarize(rep, nfiles):
    lines = []
    if rep.missing:
        lines.append('  MISSING (expected by Jan03, absent): ' + ', '.join('%s x%d' % kv for kv in sorted(rep.missing.items())))
    else:
        lines.append('  missing: none')
    if rep.unknown:
        lines.append('  UNKNOWN (present, Jan03 skips): ' + ', '.join('%s x%d' % kv for kv in sorted(rep.unknown.items())))
    else:
        lines.append('  unknown: none')
    if rep.sizebad:
        for k, v in sorted(rep.sizebad.items()):
            lines.append('  SIZE MISMATCH %s: %d cases, e.g. expected %s got %s' % (k, len(v), v[0][0], v[0][1]))
    else:
        lines.append('  size mismatches: none')
    for n in rep.notes[:8]:
        lines.append('  note: ' + n)
    return '\n'.join(lines)
