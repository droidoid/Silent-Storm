"""Extract entries of Globals.res / Waypoints.res copies into samples/packs/<Type>/<id> and compare with loose files."""
import os, sys, struct
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from ss import parse, top_main, hashmap_items
S = r'C:\a5b\recon\gaps_m\samples'
for t in ('Globals', 'Waypoints'):
    data = open(os.path.join(S, 'packs', t + '.res'), 'rb').read()
    npos = struct.unpack_from('<I', data, 4)[0]
    od = os.path.join(S, 'packs', t); os.makedirs(od, exist_ok=True)
    for cid, p in top_main(data[npos:]):
        if cid != 1: continue
        for k, v in hashmap_items(p):
            fid = struct.unpack('<i', k)[0]; st, ln = struct.unpack('<II', v)
            body = data[st:st + ln]
            open(os.path.join(od, str(fid)), 'wb').write(body)
            lp = os.path.join(S, t, str(fid))
            loose = open(lp, 'rb').read() if os.path.exists(lp) else None
            print('%s.res id=%d len=%d loose: %s' % (t, fid, ln, 'absent' if loose is None else ('identical' if loose == body else 'DIFFERENT (%d bytes)' % len(loose))))
