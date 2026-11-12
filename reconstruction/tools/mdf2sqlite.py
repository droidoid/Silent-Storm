# Converts a SQL Server 2000 database file (A5GAME_Data.MDF, the map editor's database) into a SQLite
# file, without SQL Server. The MDF is only read.
#
#   python -I mdf2sqlite.py <A5GAME_Data.MDF> <out.sqlite> [--skip TABLE ...]
#
# What is written:
#   - every user table with all rows (column names and order as in the MDF);
#   - _mdf_columns : original column types, lengths, nullability and default expressions;
#   - _mdf_objects : every object of the database (tables, views, procedures, defaults, keys ...);
#   - _mdf_source  : T-SQL text of views / procedures / functions / defaults (from syscomments);
#   - _mdf_info    : source file, page counts, per-table row counts as scanned and as recorded
#                    by the server (sysindexes.rowcnt) - the two must match.
# Types: int/smallint/tinyint/bigint/bit -> INTEGER, real/float -> REAL, (n)char/(n)varchar/(n)text
# -> TEXT, datetime -> TEXT (ISO 8601), image/binary -> BLOB. A unique index is created on ID where
# the column exists and its values are unique.
#
# The page / record parser follows recon/b_mdf/mdf2000.py (Soft/Andy/Jan03/a5dll/vs2022), extended
# with text/ntext/image (LOB) reading. Only what this database uses is supported; anything else
# stops the conversion instead of being written wrong.
import sys, os, struct, datetime, collections, sqlite3, uuid

EPOCH = datetime.datetime(1900, 1, 1)
PAGE = 8192

INT_TYPES = {48: 'B', 52: '<h', 56: '<i', 127: '<q'}
TEXT_UTF16 = (231, 239)        # nvarchar, nchar
TEXT_ANSI = (167, 175)         # varchar, char
LOB_TYPES = (34, 35, 99)       # image, text, ntext
ANSI_CODEPAGE = 'cp1251'


class Unsupported(Exception):
    pass


def f32(b):
    """float32 -> the shortest decimal that reads back as the same float32 (0.1, not 0.100000001490116)."""
    v = struct.unpack('<f', b)[0]
    if v != v or v in (float('inf'), float('-inf')):
        return v
    for digits in range(6, 10):
        s = float('%.*g' % (digits, v))
        if struct.pack('<f', s) == bytes(b):
            return s
    return v


class MDF:
    def __init__(self, fn):
        with open(fn, 'rb') as f:   # read-only
            raw = f.read()
        self.fn = fn
        self.npages = len(raw) // PAGE
        self.pages = {}
        self.byobj = collections.defaultdict(list)
        self.tornfixed = 0
        for p in range(self.npages):
            pg = bytearray(raw[p * PAGE:(p + 1) * PAGE])
            if struct.unpack_from('<I', pg, 32)[0] != p:
                continue
            if struct.unpack_from('<H', pg, 4)[0] & 0x100:   # torn page detection: restore the 2 low bits
                torn = struct.unpack_from('<I', pg, 60)[0]
                for i in range(1, 16):
                    o = i * 512 + 511
                    nb = (pg[o] & ~3) | ((torn >> (2 * i)) & 3)
                    if nb != pg[o]:
                        self.tornfixed += 1
                    pg[o] = nb
            self.pages[p] = bytes(pg)
            if pg[1] == 1:
                self.byobj[struct.unpack_from('<i', pg, 24)[0]].append(p)

    def iam_pages(self, first_iam_hex):
        """Pages allocated to an object (IAM chain): slot 0 = header + 8 single pages, slot 1 = extent bitmap."""
        pg_no = struct.unpack('<IH', bytes.fromhex(first_iam_hex))[0]
        res, seen = set(), set()
        while pg_no and pg_no not in seen and pg_no in self.pages:
            seen.add(pg_no)
            p = self.pages[pg_no]
            if p[1] != 10:
                break
            o0 = struct.unpack_from('<H', p, PAGE - 2)[0]
            o1 = struct.unpack_from('<H', p, PAGE - 4)[0]
            start = struct.unpack_from('<I', p, o0 + 40)[0]
            for i in range(8):
                sp = struct.unpack_from('<I', p, o0 + 46 + 6 * i)[0]
                if sp:
                    res.add(sp)
            ln = struct.unpack_from('<H', p, o1 + 2)[0]
            for bi, byte in enumerate(p[o1 + 4:o1 + ln]):
                for bit in range(8):
                    if byte >> bit & 1:
                        e = start + (bi * 8 + bit) * 8
                        res.update(range(e, e + 8))
            pg_no = struct.unpack_from('<I', p, 16)[0]
        return res

    def records(self, obj, allowed=None, stats=None):
        for p in sorted(self.byobj.get(obj, [])):
            if allowed is not None and p not in allowed:
                continue
            pg = self.pages[p]
            for sl in range(struct.unpack_from('<H', pg, 22)[0]):
                off = struct.unpack_from('<H', pg, PAGE - 2 - 2 * sl)[0]
                if off == 0:
                    continue
                rt = (pg[off] >> 1) & 7
                if stats is not None:
                    stats[rt] += 1
                if rt in (0, 1):          # primary / forwarded data record
                    yield pg, off

    def raw_fields(self, pg, off, cols):
        """Column name -> raw bytes (None for NULL / not stored)."""
        a = pg[off]
        fe = struct.unpack_from('<H', pg, off + 2)[0]
        ncol = struct.unpack_from('<H', pg, off + fe)[0]
        nbl = (ncol + 7) // 8
        nb = pg[off + fe + 2:off + fe + 2 + nbl]
        p = off + fe + 2 + nbl
        varends = []
        if a & 0x20:
            nv = struct.unpack_from('<H', pg, p)[0]
            p += 2
            varends = [struct.unpack_from('<H', pg, p + 2 * i)[0] for i in range(nv)]
            p += 2 * nv
        vstart = p - off
        res = {}
        for c in cols:
            ci = c['colid'] - 1
            xo = c['xoffset']
            # A column added by ALTER TABLE after the row was written is not in the row at all: the
            # row's own column count / fixed part is shorter. Such a value is NULL.
            if ci >= ncol or ((nb[ci // 8] >> (ci % 8)) & 1) or xo == 0:
                res[c['name']] = None
            elif xo > 0:
                res[c['name']] = pg[off + xo:off + xo + c['length']] if xo + c['length'] <= fe else None
            else:
                k = -xo - 1
                if k >= len(varends):
                    res[c['name']] = None
                else:
                    st = vstart if k == 0 else (varends[k - 1] & 0x7fff)
                    res[c['name']] = pg[off + st:off + (varends[k] & 0x7fff)]
        return res

    # ---- text / ntext / image -----------------------------------------------------------------
    def _blob_record(self, page, slot):
        pg = self.pages.get(page)
        if pg is None or pg[1] not in (3, 4):
            raise Unsupported('LOB pointer to page %d which is not a text page' % page)
        if slot >= struct.unpack_from('<H', pg, 22)[0]:
            raise Unsupported('LOB pointer to missing slot %d:%d' % (page, slot))
        off = struct.unpack_from('<H', pg, PAGE - 2 - 2 * slot)[0]
        length = struct.unpack_from('<H', pg, off + 2)[0]
        return pg[off:off + length]

    def _blob_node(self, page, slot):
        r = self._blob_record(page, slot)
        kind = struct.unpack_from('<H', r, 12)[0]
        if kind == 0:                                   # SMALL_ROOT: size(2) unused(4) data
            size = struct.unpack_from('<H', r, 14)[0]
            return r[20:20 + size]
        if kind == 3:                                   # DATA
            return r[14:]
        if kind == 8:                                   # NULL: the value was set to NULL, the record stays
            return b''
        if kind in (4, 1, 5):                           # ROOT: maxlinks curlinks level unused(4), links of 12
            links, base, step, rid = struct.unpack_from('<H', r, 16)[0], 24, 12, 4
        elif kind == 2:                                 # INTERNAL: maxlinks curlinks level, links of 16
            links, base, step, rid = struct.unpack_from('<H', r, 16)[0], 20, 16, 8
        else:
            raise Unsupported('LOB record type %d at %d:%d' % (kind, page, slot))
        out = bytearray()
        for i in range(links):
            o = base + i * step
            end = struct.unpack_from('<I', r, o)[0]     # cumulative size after this child
            cpage, cfile, cslot = struct.unpack_from('<IHH', r, o + rid)
            out += self._blob_node(cpage, cslot)
            if len(out) != end:
                raise Unsupported('LOB size mismatch at %d:%d link %d: got %d, expected %d' % (page, slot, i, len(out), end))
        return bytes(out)

    def lob(self, ptr):
        """16-byte in-row text pointer -> bytes."""
        if len(ptr) != 16:
            raise Unsupported('text pointer of %d bytes' % len(ptr))
        stamp, page, fileno, slot = struct.unpack('<QIHH', ptr)
        if page == 0 and stamp == 0:
            return b''
        r = self._blob_record(page, slot)
        if struct.unpack_from('<Q', r, 4)[0] != stamp:
            raise Unsupported('LOB id mismatch at %d:%d' % (page, slot))
        return self._blob_node(page, slot)

    # ---- values --------------------------------------------------------------------------------
    def value(self, c, b):
        if b is None:
            return None
        t = c['xtype']
        if t in INT_TYPES:
            return struct.unpack(INT_TYPES[t], b)[0]
        if t == 104:
            return (b[0] >> (c.get('bitpos') or 0)) & 1
        if t == 59:
            return f32(b)
        if t == 62:
            return struct.unpack('<d', b)[0]
        if t in TEXT_UTF16:
            return b.decode('utf-16le')
        if t in TEXT_ANSI:
            return b.decode(ANSI_CODEPAGE)
        if t == 61:
            ticks, days = struct.unpack('<Ii', b)
            return (EPOCH + datetime.timedelta(days=days, seconds=ticks / 300.0)).isoformat(sep=' ', timespec='milliseconds')
        if t == 58:
            mins, days = struct.unpack('<HH', b)
            return (EPOCH + datetime.timedelta(days=days, minutes=mins)).isoformat(sep=' ')
        if t == 36:
            return str(uuid.UUID(bytes_le=bytes(b)))
        if t == 99:
            return self.lob(b).decode('utf-16le')
        if t == 35:
            return self.lob(b).decode(ANSI_CODEPAGE)
        if t == 34:
            return self.lob(b)
        if t in (165, 173):
            return bytes(b)
        raise Unsupported('column type %d' % t)


def sqlite_type(xtype):
    if xtype in INT_TYPES or xtype == 104:
        return 'INTEGER'
    if xtype in (59, 62):
        return 'REAL'
    if xtype in (34, 165, 173):
        return 'BLOB'
    return 'TEXT'


SYSCOLUMNS_BOOT = [dict(name='id', colid=2, xoffset=4, length=4, xtype=56),
                   dict(name='xtype', colid=3, xoffset=8, length=1, xtype=48),
                   dict(name='xusertype', colid=5, xoffset=10, length=2, xtype=52),
                   dict(name='length', colid=6, xoffset=12, length=2, xtype=52),
                   dict(name='colid', colid=9, xoffset=16, length=2, xtype=52),
                   dict(name='xoffset', colid=10, xoffset=18, length=2, xtype=52),
                   dict(name='bitpos', colid=11, xoffset=20, length=1, xtype=48),
                   dict(name='name', colid=1, xoffset=-1, length=256, xtype=231)]


def system_rows(m, obj, cols):
    """Rows of a system table; columns of types this tool does not decode come back as raw bytes."""
    for pg, off in m.records(obj):
        raw = m.raw_fields(pg, off, cols)
        row = {}
        for c in cols:
            try:
                row[c['name']] = m.value(c, raw[c['name']])
            except (Unsupported, struct.error, UnicodeDecodeError):
                row[c['name']] = bytes(raw[c['name']])
        yield row


def q(name):
    return '"' + name.replace('"', '""') + '"'


def convert(mdf_path, out_path, skip=()):
    m = MDF(mdf_path)
    boot = collections.defaultdict(list)
    for r in system_rows(m, 3, SYSCOLUMNS_BOOT):
        boot[r['id']].append(r)
    for k in boot:
        boot[k].sort(key=lambda r: r['colid'])
    cols = collections.defaultdict(list)                 # full syscolumns rows
    for r in system_rows(m, 3, boot[3]):
        cols[r['id']].append(r)
    for k in cols:
        cols[k].sort(key=lambda r: r['colid'])
    objs = {r['id']: r for r in system_rows(m, 1, boot[1])}
    types = {r['xusertype']: r['name'] for r in system_rows(m, 4, boot[4])}
    idx = collections.defaultdict(list)
    for r in system_rows(m, 2, boot[2]):
        idx[r['id']].append(r)
    source = collections.defaultdict(list)               # syscomments: object id -> text pieces
    for r in system_rows(m, 6, boot[6]):
        text = r.get('ctext')
        if isinstance(text, (bytes, bytearray)):   # status bit 2: stored as 8-bit text, otherwise UTF-16
            text = bytes(text).decode(ANSI_CODEPAGE if (r.get('status') or 0) & 2 else 'utf-16le', errors='replace')
        source[r['id']].append((r.get('number') or 0, r.get('colid') or 0, text or ''))
    source = {k: ''.join(t for _, _, t in sorted(v)) for k, v in source.items()}

    if os.path.exists(out_path):
        os.remove(out_path)
    db = sqlite3.connect(out_path)
    db.execute('CREATE TABLE _mdf_info (key TEXT, value TEXT)')
    db.execute('CREATE TABLE _mdf_columns ("table" TEXT, colid INTEGER, name TEXT, type TEXT, length INTEGER, '
               'nullable INTEGER, "default" TEXT)')
    db.execute('CREATE TABLE _mdf_objects (id INTEGER, name TEXT, xtype TEXT, parent TEXT, created TEXT)')
    db.execute('CREATE TABLE _mdf_source (name TEXT, xtype TEXT, parent TEXT, text TEXT)')
    db.execute('CREATE TABLE _mdf_tables ("table" TEXT, rows INTEGER, rows_recorded INTEGER, unique_id INTEGER)')

    for o in sorted(objs.values(), key=lambda o: (str(o['xtype']), str(o['name']).lower())):
        parent = objs.get(o.get('parent_obj'), {}).get('name')
        xt = (o['xtype'] or '').strip()
        db.execute('INSERT INTO _mdf_objects VALUES (?,?,?,?,?)', (o['id'], o['name'], xt, parent, o.get('crdate')))
        if o['id'] in source and xt != 'S':
            db.execute('INSERT INTO _mdf_source VALUES (?,?,?,?)', (o['name'], xt, parent, source[o['id']]))

    tables = sorted((o for o in objs.values() if (o['xtype'] or '').strip() == 'U'), key=lambda o: o['name'].lower())
    total = 0
    problems = []
    for o in tables:
        name, oid = o['name'], o['id']
        if name in skip:
            continue
        cl = cols[oid]
        for c in cl:
            db.execute('INSERT INTO _mdf_columns VALUES (?,?,?,?,?,?,?)',
                       (name, c['colid'], c['name'], types.get(c['xusertype'], 'x%d' % c['xtype']), c['length'],
                        0 if (c.get('typestat') or 0) & 1 else 1, source.get(c.get('cdefault'))))   # typestat bit 0 = NOT NULL
        db.execute('CREATE TABLE %s (%s)' % (q(name), ', '.join('%s %s' % (q(c['name']), sqlite_type(c['xtype'])) for c in cl)))
        allowed = set()
        for i in idx[oid]:
            iam = i.get('FirstIAM')
            if isinstance(iam, (bytes, bytearray)):
                iam = bytes(iam).hex()
            if i.get('indid') in (0, 1) and iam and iam != '000000000000':
                allowed |= m.iam_pages(iam)
        rows = []
        for pg, off in m.records(oid, allowed):
            raw = m.raw_fields(pg, off, cl)
            try:
                rows.append(tuple(m.value(c, raw[c['name']]) for c in cl))
            except (Unsupported, struct.error, UnicodeDecodeError) as e:
                raise SystemExit('%s: cannot decode a row: %s' % (name, e))
        db.executemany('INSERT INTO %s VALUES (%s)' % (q(name), ','.join('?' * len(cl))), rows)
        recorded = [i.get('rowcnt') for i in idx[oid] if i.get('indid') in (0, 1)]
        recorded = recorded[0] if recorded else None
        if recorded is not None and recorded != len(rows):
            problems.append('%s: %d rows read, the server recorded %d' % (name, len(rows), recorded))
        unique_id = 0
        id_col = [c['name'] for c in cl if c['name'].lower() == 'id']
        if id_col:
            ids = [r[[c['name'] for c in cl].index(id_col[0])] for r in rows]
            if None not in ids and len(set(ids)) == len(ids):
                db.execute('CREATE UNIQUE INDEX %s ON %s (%s)' % (q('ux_' + name + '_ID'), q(name), q(id_col[0])))
                unique_id = 1
        db.execute('INSERT INTO _mdf_tables VALUES (?,?,?,?)', (name, len(rows), recorded, unique_id))
        total += len(rows)

    for k, v in (('source', os.path.abspath(mdf_path)), ('pages', m.npages), ('pages_valid', len(m.pages)),
                 ('torn_bytes_restored', m.tornfixed), ('user_tables', len(tables) - len([t for t in tables if t['name'] in skip])),
                 ('rows', total), ('skipped', ','.join(skip))):
        db.execute('INSERT INTO _mdf_info VALUES (?,?)', (k, str(v)))
    db.commit()
    db.close()
    print('%s: %d tables, %d rows -> %s' % (os.path.basename(mdf_path), len(tables) - len(skip), total, out_path))
    for p in problems:
        print('ROW COUNT MISMATCH  ' + p)
    return not problems


def main():
    args = sys.argv[1:]
    skip = ()
    if '--skip' in args:
        i = args.index('--skip')
        skip, args = tuple(args[i + 1:]), args[:i]
    if len(args) != 2:
        raise SystemExit('usage: python -I mdf2sqlite.py <A5GAME_Data.MDF> <out.sqlite> [--skip TABLE ...]')
    sys.exit(0 if convert(args[0], args[1], skip) else 1)


if __name__ == '__main__':
    main()
