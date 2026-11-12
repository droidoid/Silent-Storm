# Read-only SQL Server 2000 MDF parser (data pages only). Usage: python -I mdf2000.py <mdf> <outprefix>
import sys, struct, datetime, collections, json, uuid
EPOCH = datetime.datetime(1900, 1, 1)


def dt(b):
    ticks, days = struct.unpack('<Ii', b)
    try:
        return EPOCH + datetime.timedelta(days=days, seconds=ticks / 300.0)
    except Exception:
        return None


def sdt(b):
    mins, days = struct.unpack('<HH', b)
    return EPOCH + datetime.timedelta(days=days, minutes=mins)


class MDF:
    def __init__(s, fn):
        with open(fn, 'rb') as f:   # read-only
            raw = f.read()
        s.fn = fn; s.npages = len(raw) // 8192; s.pages = {}
        s.byobj = collections.defaultdict(list)
        s.tornfixed = 0
        for p in range(s.npages):
            pg = bytearray(raw[p * 8192:(p + 1) * 8192])
            if struct.unpack_from('<I', pg, 32)[0] != p:
                continue
            flag = struct.unpack_from('<H', pg, 4)[0]
            if flag & 0x100:
                torn = struct.unpack_from('<I', pg, 60)[0]
                for i in range(1, 16):
                    o = i * 512 + 511
                    nb = (pg[o] & ~3) | ((torn >> (2 * i)) & 3)
                    if nb != pg[o]:
                        s.tornfixed += 1
                    pg[o] = nb
            s.pages[p] = bytes(pg)
            if pg[1] == 1:
                s.byobj[struct.unpack_from('<i', pg, 24)[0]].append(p)

    def iam_pages(s, first_iam_hex):
        """Pages allocated per IAM chain (SQL 2000 layout: slot0 = IAM header, singles at +46, slot1 = extent bitmap)."""
        pg_no = struct.unpack('<IH', bytes.fromhex(first_iam_hex))[0]
        res = set(); seen = set()
        while pg_no and pg_no not in seen and pg_no in s.pages:
            seen.add(pg_no)
            p = s.pages[pg_no]
            if p[1] != 10:
                break
            o0 = struct.unpack_from('<H', p, 8190)[0]
            o1 = struct.unpack_from('<H', p, 8188)[0]
            start = struct.unpack_from('<I', p, o0 + 40)[0]
            for i in range(8):
                sp = struct.unpack_from('<I', p, o0 + 46 + 6 * i)[0]
                if sp:
                    res.add(sp)
            ln = struct.unpack_from('<H', p, o1 + 2)[0]
            bm = p[o1 + 4:o1 + ln]
            for bi, byte in enumerate(bm):
                if byte:
                    for bit in range(8):
                        if byte >> bit & 1:
                            e = start + (bi * 8 + bit) * 8
                            res.update(range(e, e + 8))
            pg_no = struct.unpack_from('<I', p, 16)[0]
        return res

    def records(s, obj, stats=None, allowed=None):
        for p in sorted(s.byobj.get(obj, [])):
            if allowed is not None and p not in allowed:
                continue
            pg = s.pages[p]; sc = struct.unpack_from('<H', pg, 22)[0]
            for sl in range(sc):
                off = struct.unpack_from('<H', pg, 8190 - 2 * sl)[0]
                if off == 0:
                    continue
                a = pg[off]; rt = (a >> 1) & 7
                if stats is not None:
                    stats[rt] += 1
                if rt in (0, 1):
                    yield pg, off, (p, sl)

    def parse(s, pg, off, cols):
        a = pg[off]; fe = struct.unpack_from('<H', pg, off + 2)[0]
        ncol = struct.unpack_from('<H', pg, off + fe)[0]
        nbl = (ncol + 7) // 8; nb = pg[off + fe + 2:off + fe + 2 + nbl]
        p = off + fe + 2 + nbl; varends = []
        if a & 0x20:
            nv = struct.unpack_from('<H', pg, p)[0]; p += 2
            varends = [struct.unpack_from('<H', pg, p + 2 * i)[0] for i in range(nv)]; p += 2 * nv
        vstart = p - off
        res = {}
        for c in cols:
            ci = c['colid'] - 1
            isnull = (ci // 8 < len(nb)) and (nb[ci // 8] >> (ci % 8)) & 1
            if isnull:
                res[c['name']] = None; continue
            xo = c['xoffset']
            if xo == 0:   # computed (not stored)
                res[c['name']] = None; continue
            if xo > 0:
                b = pg[off + xo:off + xo + c['length']]
            else:
                k = -xo - 1
                if k >= len(varends):
                    res[c['name']] = None; continue
                st = vstart if k == 0 else (varends[k - 1] & 0x7fff)
                en = varends[k] & 0x7fff
                b = pg[off + st:off + en]
            res[c['name']] = decode(c, b)
        return res


def decode(c, b):
    t = c['xtype']
    try:
        if t == 48: return b[0]
        if t == 52: return struct.unpack('<h', b)[0]
        if t == 56: return struct.unpack('<i', b)[0]
        if t == 127: return struct.unpack('<q', b)[0]
        if t == 62: return struct.unpack('<d', b)[0]
        if t == 59: return struct.unpack('<f', b)[0]
        if t == 61: return dt(b)
        if t == 58: return sdt(b)
        if t == 104: return (b[0] >> c.get('bitpos', 0)) & 1
        if t in (231, 239): return b.decode('utf-16le', errors='replace')
        if t in (175, 167): return b.decode('cp1251', errors='replace')
        if t == 36: return str(uuid.UUID(bytes_le=bytes(b)))
        return bytes(b).hex()
    except Exception:
        return 'ERR:' + bytes(b).hex()


SC_BOOT = [dict(name='id', colid=2, xoffset=4, length=4, xtype=56),
           dict(name='xtype', colid=3, xoffset=8, length=1, xtype=48),
           dict(name='xusertype', colid=5, xoffset=10, length=2, xtype=52),
           dict(name='length', colid=6, xoffset=12, length=2, xtype=52),
           dict(name='colid', colid=9, xoffset=16, length=2, xtype=52),
           dict(name='xoffset', colid=10, xoffset=18, length=2, xtype=52),
           dict(name='bitpos', colid=11, xoffset=20, length=1, xtype=48),
           dict(name='name', colid=1, xoffset=-1, length=256, xtype=231)]


def load(fn):
    m = MDF(fn)
    cols = collections.defaultdict(list)
    for pg, off, loc in m.records(3):
        r = m.parse(pg, off, SC_BOOT)
        cols[r['id']].append(r)
    for k in cols:
        cols[k].sort(key=lambda r: r['colid'])
    objs = {}
    for pg, off, loc in m.records(1):
        r = m.parse(pg, off, cols[1]); objs[r['id']] = r
    types = {}
    for pg, off, loc in m.records(4):
        r = m.parse(pg, off, cols[4]); types[r['xusertype']] = r['name']
    idx = collections.defaultdict(list)
    for pg, off, loc in m.records(2):
        r = m.parse(pg, off, cols[2]); idx[r['id']].append(r)
    return m, cols, objs, types, idx


def main():
    fn, out = sys.argv[1], sys.argv[2]
    m, cols, objs, types, idx = load(fn)
    L = []
    L.append('FILE %s  pages=%d  pages-with-valid-header=%d  torn-bit bytes restored=%d' % (fn, m.npages, len(m.pages), m.tornfixed))
    L.append('sysobjects physical cols: ' + ', '.join('%s@%d' % (c['name'], c['xoffset']) for c in cols[1]))
    L.append('syscolumns physical cols: ' + ', '.join('%s@%d' % (c['name'], c['xoffset']) for c in cols[3]))
    utabs = sorted([o for o in objs.values() if (o['xtype'] or '').strip() == 'U'], key=lambda o: o['name'].lower())
    other = collections.Counter((o['xtype'] or '').strip() for o in objs.values())
    L.append('object counts by xtype: ' + str(dict(other)))
    L.append('user tables: %d' % len(utabs))
    L.append('')
    summary = {}
    alldates = []
    for o in utabs:
        oid = o['id']; cl = cols[oid]
        st = collections.Counter(); n = 0; dmax = None; dmin = None; dcount = 0
        dcols = [c for c in cl if c['xtype'] in (61, 58)]
        allowed = set()
        for i in idx[oid]:
            if i.get('indid') in (0, 1) and i.get('FirstIAM') and i['FirstIAM'] != '000000000000':
                allowed |= m.iam_pages(i['FirstIAM'])
        stale = len([p for p in m.byobj.get(oid, []) if p not in allowed])
        for pg, off, loc in m.records(oid, st, allowed):
            n += 1
            if dcols:
                try:
                    r = m.parse(pg, off, dcols)
                    for k, v in r.items():
                        if isinstance(v, datetime.datetime):
                            dcount += 1; alldates.append((v, o['name'] + '.' + k))
                            dmax = v if dmax is None or v > dmax else dmax
                            dmin = v if dmin is None or v < dmin else dmin
                except Exception:
                    pass
        rc = [i.get('rowcnt') for i in idx[oid] if i.get('indid') in (0, 1)]
        L.append('TABLE %s  id=%d  crdate=%s  rows(scan)=%d  sysindexes.rowcnt=%s  datapages=%d (+%d stale unallocated)  rectypes=%s%s' % (
            o['name'], oid, o.get('crdate'), n, rc, len(m.byobj.get(oid, [])) - stale, stale, dict(st),
            ('  datetime[%d vals] %s .. %s' % (dcount, dmin, dmax)) if dcount else ''))
        for c in cl:
            L.append('    %-3d %-40s %-16s len=%d' % (c['colid'], c['name'], types.get(c['xusertype'], 'x%d' % c['xtype']), c['length']))
        summary[o['name']] = {'id': oid, 'rows': n, 'crdate': str(o.get('crdate')), 'refdate': str(o.get('refdate')),
                              'cols': [[c['name'], types.get(c['xusertype'], 'x%d' % c['xtype']), c['length']] for c in cl]}
    L.append('')
    L.append('NON-TABLE OBJECTS:')
    for o in sorted(objs.values(), key=lambda o: ((o['xtype'] or ''), o['name'].lower())):
        if (o['xtype'] or '').strip() != 'U':
            L.append('  %-3s %-50s id=%-11d crdate=%s refdate=%s parent=%s' % (o['xtype'], o['name'], o['id'], o.get('crdate'), o.get('refdate'), o.get('parent_obj')))
    alldates.sort()
    L.append('')
    L.append('newest 25 datetime values in user tables:')
    for v, t in alldates[-25:]:
        L.append('   %s  %s' % (v, t))
    L.append('oldest 5 datetime values in user tables:')
    for v, t in alldates[:5]:
        L.append('   %s  %s' % (v, t))
    with open(out + '.txt', 'w', encoding='utf-8') as f:
        f.write('\n'.join(L))
    with open(out + '.json', 'w') as f:
        json.dump(summary, f, indent=0)
    print('\n'.join(L[:6]))


if __name__ == '__main__':
    main()
