"""Minimal PDB 7.0 (MSF) reader: info stream GUID/age, public symbols (S_PUB32).
usage: python pdb.py info <pdb>...
       python pdb.py pubs <pdb> > pubs.txt
       python pdb.py find <exe> <pdb> <hex-dword>...   (functions whose code contains the dword as an immediate)
"""
import struct, sys, uuid, bisect


class PDB:
    def __init__(self, path):
        f = open(path, 'rb')
        self.f = f
        hdr = f.read(56)
        assert hdr.startswith(b'Microsoft C/C++ MSF 7.00'), 'not a PDB 7.0'
        self.bs, _, self.nblocks, dirbytes, _, mapaddr = struct.unpack_from('<IIIIII', hdr, 32)
        nb = (dirbytes + self.bs - 1) // self.bs
        f.seek(mapaddr * self.bs)
        dirblocks = struct.unpack('<%dI' % nb, f.read(4 * nb))
        d = b''.join(self.block(b) for b in dirblocks)[:dirbytes]
        n = struct.unpack_from('<I', d, 0)[0]
        sizes = struct.unpack_from('<%dI' % n, d, 4)
        pos = 4 + 4 * n
        self.streams = []
        for s in sizes:
            if s == 0xFFFFFFFF:
                s = 0
            k = (s + self.bs - 1) // self.bs
            blocks = struct.unpack_from('<%dI' % k, d, pos)
            pos += 4 * k
            self.streams.append((s, blocks))

    def block(self, b):
        self.f.seek(b * self.bs)
        return self.f.read(self.bs)

    def stream(self, i):
        s, blocks = self.streams[i]
        return b''.join(self.block(b) for b in blocks)[:s]

    def info(self):
        s = self.stream(1)
        ver, sig, age = struct.unpack_from('<III', s, 0)
        return ver, age, uuid.UUID(bytes_le=s[12:28])

    def publics(self):
        dbi = self.stream(3)
        symrec = struct.unpack_from('<H', dbi, 20)[0]
        data = self.stream(symrec)
        pos, out = 0, []
        while pos + 4 <= len(data):
            ln, typ = struct.unpack_from('<HH', data, pos)
            if typ == 0x110E:   # S_PUB32
                flags, off, seg = struct.unpack_from('<IIH', data, pos + 4)
                name = data[pos + 14:data.index(b'\0', pos + 14)].decode('latin-1')
                out.append((seg, off, name, flags))
            pos += 2 + ln
        return out


def sections(exe):
    d = open(exe, 'rb').read()
    pe = struct.unpack_from('<I', d, 0x3C)[0]
    nsec = struct.unpack_from('<H', d, pe + 6)[0]
    opt = struct.unpack_from('<H', d, pe + 20)[0]
    base = struct.unpack_from('<I', d, pe + 24 + 28)[0]
    secs = []
    for i in range(nsec):
        o = pe + 24 + opt + 40 * i
        name = d[o:o + 8].rstrip(b'\0').decode()
        vsize, va, rawsize, rawptr = struct.unpack_from('<IIII', d, o + 8)
        secs.append((name, va, vsize, rawptr, rawsize))
    return d, base, secs


if __name__ == '__main__':
    cmd = sys.argv[1]
    if cmd == 'info':
        for p in sys.argv[2:]:
            ver, age, g = PDB(p).info()
            print('%s: version %d age %d guid {%s}' % (p, ver, age, str(g).upper()))
    elif cmd == 'pubs':
        for seg, off, name, fl in PDB(sys.argv[2]).publics():
            print('%d:%08X %s' % (seg, off, name))
    elif cmd == 'find':
        d, base, secs = sections(sys.argv[2])
        pubs = sorted((seg, off, name) for seg, off, name, _ in PDB(sys.argv[3]).publics())
        keys = [(s, o) for s, o, _ in pubs]
        for h in sys.argv[4:]:
            needle = struct.pack('<I', int(h, 16))
            for si, (sn, va, vs, rp, rs) in enumerate(secs, 1):
                raw = d[rp:rp + rs]
                i = raw.find(needle)
                while i >= 0:
                    k = bisect.bisect_right(keys, (si, i)) - 1
                    fn = pubs[k][2] if k >= 0 and pubs[k][0] == si else '?'
                    print('0x%s in %s+0x%X (va 0x%08X): %s' % (h, sn, i, base + va + i, fn))
                    i = raw.find(needle, i + 1)
