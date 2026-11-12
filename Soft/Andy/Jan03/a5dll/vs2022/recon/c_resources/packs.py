"""Static check of *.res packs against Jan03 FileIO/FilesPackage.cpp:
<sig:4 = 0x95938921><nPos:4> ... header at nPos = CStructureSaver stream: chunk1 = hash_map<FILE_ID,SFileInfo{start,len}>,
chunk2 = hash_map<FILE_ID,FILETIME> (only read by Update). Each entry's bytes are then parsed as a resource."""
import sys, os, struct
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from ss import parse, top_main, hashmap_items

JAN03_SIG = 0x95938921
d = sys.argv[1]
for fn in sorted(os.listdir(d)):
    data = open(os.path.join(d, fn), 'rb').read()
    sig = struct.unpack_from('<I', data, 0)[0]
    print('%s: %d bytes, signature 0x%08X (%s)' % (fn, len(data), sig, 'Jan03 OK' if sig == JAN03_SIG else 'Jan03 expects 0x%08X -> "wrong signature" exception, OpenFilesPackage returns 0' % JAN03_SIG))
    if len(data) < 8:
        print('   no header offset (signature only)')
        continue
    npos = struct.unpack_from('<I', data, 4)[0]
    if npos > len(data):
        print('   nPos=%d beyond EOF -> not a Jan03-style pack' % npos)
        continue
    hdr = data[npos:]
    top = parse(hdr)
    if top is None:
        print('   header at %d (%d bytes) does not parse as chunks' % (npos, len(hdr)))
        continue
    print('   header at %d, %d bytes, top chunks %s' % (npos, len(hdr), [(c, len(p)) for c, p in top]))
    main = top_main(hdr)
    files = None
    for cid, p in main:
        if cid == 1:
            items = hashmap_items(p)
            files = []
            for k, v in items:
                fid = struct.unpack('<i', k)[0]
                files.append((fid, v))
    if files is None:
        print('   no chunk 1 (file table)')
        continue
    ok = 0
    out = []
    for fid, v in files:
        if len(v) == 8:
            st, ln = struct.unpack('<II', v)
            inb = st + ln <= npos and st >= 8
            body = data[st:st + ln]
            pp = parse(body)
            ok += inb
            out.append((fid, st, ln, inb, pp is not None, body[:6].hex()))
        else:
            out.append((fid, 'value size %d != 8 (SFileInfo)' % len(v)))
    print('   %d entries; SFileInfo 8 bytes and inside data area: %d' % (len(files), ok))
    for o in out[:6]:
        print('     ', o)
    if all(len(o) == 6 for o in out):
        covered = sum(o[2] for o in out)
        print('   sum of entry lengths %d vs data area %d; entries parsing as chunk streams: %d/%d' % (covered, npos - 8, sum(o[4] for o in out), len(out)))
