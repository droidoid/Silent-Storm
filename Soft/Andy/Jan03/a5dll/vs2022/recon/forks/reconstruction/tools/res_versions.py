"""Per resource dir of a game data tree: count loose files by top-level CStructureSaver version (chunk 4) and pack marker (chunk 3).
usage: python res_versions.py <Complete dir> [sample_per_dir]"""
import os, struct, sys, collections
root = sys.argv[1]; N = int(sys.argv[2]) if len(sys.argv) > 2 else 400
def top(path):
    with open(path, 'rb') as f: d = f.read(1 << 16)
    pos, ids, ver = 0, [], None
    while pos + 2 <= len(d) and len(ids) < 8:
        cid = d[pos]; b = d[pos + 1]
        if b & 1:
            if pos + 5 > len(d): break
            ln = struct.unpack_from('<I', d, pos + 1)[0] >> 1; hdr = 5
        else: ln = b >> 1; hdr = 2
        ids.append(cid)
        if cid == 4 and ln >= 4 and pos + hdr + 4 <= len(d): ver = struct.unpack_from('<i', d, pos + hdr)[0]
        pos += hdr + ln
    return ids, ver
for dn in sorted(os.listdir(root)):
    p = os.path.join(root, dn)
    if not os.path.isdir(p) or dn in ('cfg', 'res', 'temp', 'Garbage', 'Sounds', 'Sounds-PCM-bckp', 'VIDEO', 'VOICEOVER', 'Scripts', 'Textures', 'LRTextures'): continue
    fs = [f for f in os.listdir(p) if os.path.isfile(os.path.join(p, f))]
    c = collections.Counter()
    for f in sorted(fs)[:N]:
        try:
            ids, ver = top(os.path.join(p, f))
            c['v%s%s' % (ver if ver is not None else 0, '+packed' if 3 in ids else '')] += 1
        except Exception as e: c['err'] += 1
    print('%-14s files=%6d sampled=%4d %s' % (dn, len(fs), min(N, len(fs)), dict(c)))
