"""Runs stand2.exe on every file of each type in samples/ (chunks of 400 ids); output -> stand_<Type>.txt"""
import os, subprocess, sys
S = r'C:\a5b\recon\gaps_m\samples'; EXE = r'C:\a5b\recon\gaps_m\stand\out\stand2.exe'
for t in sys.argv[1:]:
    ids = sorted(int(f) for f in os.listdir(os.path.join(S, t)) if f.isdigit())
    out = open(r'C:\a5b\recon\gaps_m\stand_%s.txt' % t, 'w')
    for i in range(0, len(ids), 400):
        r = subprocess.run([EXE, t] + [str(x) for x in ids[i:i + 400]], cwd=S, capture_output=True, text=True, timeout=600)
        out.write(r.stdout); out.write(r.stderr)
        if r.returncode: out.write('EXIT %d\n' % r.returncode)
    out.close()
    print(t, len(ids))
