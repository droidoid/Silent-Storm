import sys, struct, datetime
E = datetime.datetime(1900, 1, 1)
for fn in sys.argv[1:]:
    with open(fn, 'rb') as f:
        f.seek(0); p0 = f.read(8192); f.seek(9 * 8192); b = f.read(8192)
    print('==', fn)
    print(' p0 type=%d objId=%d  p9 type=%d objId=%d' % (p0[1], struct.unpack_from('<i', p0, 24)[0], b[1], struct.unpack_from('<i', b, 24)[0]))
    r = 96
    print(' boot rec @0x60: w0=%d dbi_version=%d dbi_createVersion=%d' % struct.unpack_from('<HHH', b, r + 2))
    name = b[0x94:0x194].decode('utf-16le').rstrip()
    print(' dbi_dbname=%r  dbid(?)@0x194=%d' % (name, struct.unpack_from('<H', b, 0x194)[0]))
    for o in range(96, 1024):
        t, d = struct.unpack_from('<Ii', b, o)
        if 36000 < d < 38500 and t < 25920000:
            print('  datetime-like @0x%x (rec+%d): %s' % (o, o - 96, E + datetime.timedelta(days=d, seconds=t / 300)))
    # file header page 0 strings
    for o in range(96, 2048):
        t, d = struct.unpack_from('<Ii', p0, o)
        if 36000 < d < 38500 and t < 25920000:
            print('  p0 datetime-like @0x%x: %s' % (o, E + datetime.timedelta(days=d, seconds=t / 300)))
    s = p0[96:2048]
    import re
    for m in re.finditer(rb'(?:[\x20-\x7e]\x00){4,}', s):
        print('  p0 utf16 @0x%x: %s' % (m.start() + 96, m.group().decode('utf-16le')))
