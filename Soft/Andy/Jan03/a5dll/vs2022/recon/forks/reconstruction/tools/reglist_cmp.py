"""Extract pRegList names from a ScriptFunctions.cpp and compare with release list.
usage: python reglist_cmp.py <recon ScriptFunctions.cpp> <jan03 ScriptFunctions.cpp> <release_reglist.txt> <release exe> <scripts tsv> <recon src root> <jan03 src root>"""
import re, sys, os
def reg(path):
    t = open(path, encoding='cp1251', errors='replace').read()
    s = t.index('pRegList[] =')
    e = t.index('};', s)
    body = t[s:e]
    body = re.sub(r'//[^\n]*', '', body)
    body = re.sub(r'/\*.*?\*/', '', body, flags=re.S)
    names = re.findall(r'REG_FUNCTION\(\s*(\w+)\s*\)', body) + re.findall(r'\{\s*"(\w+)"', body)
    if '#if' in body: print('WARNING: preprocessor in reglist', path, file=sys.stderr)
    return names
rc, jc, rel, exe, scr, rroot, jroot = sys.argv[1:8]
R = reg(rc); J = reg(jc)
REL = [l.split('\t')[0].lstrip('﻿') for l in open(rel, encoding='utf-8-sig') if l.strip()]
sR, sJ, sREL = set(R), set(J), set(REL)
print('recon reglist entries', len(R), 'unique', len(sR), '| jan03', len(sJ), '| release', len(sREL))
print('release covered by recon:', len(sREL & sR), '/', len(sREL))
print('release-only-vs-jan03 (', len(sREL - sJ), ') covered by recon:', len((sREL - sJ) & sR))
print('MISSING in recon (release names):', ' '.join(sorted(sREL - sR)))
print('recon-only (not in release):', ' '.join(sorted(sR - sREL)))
# breakage list: functions scripts call but jan03 lacks
# On* callbacks: names in release exe strings that are defined as functions in scripts
data = open(exe, 'rb').read()
exe_on = set(m.decode() for m in re.findall(rb'\x00(On[A-Z][A-Za-z0-9_]{2,40})\x00', data))
stxt = open(scr, encoding='utf-8', errors='replace').read()
defined_on = set(re.findall(r'function\s+(On[A-Z]\w*)\s*\(', stxt))
cb = sorted(exe_on & defined_on)
print('\nOn* callbacks (release exe string AND defined in game.db scripts):', len(cb), ' '.join(cb))
def grep_str(root, name):
    pat = re.compile(r'"' + name + r'"')
    hits = []
    for dp, dn, fn in os.walk(root):
        if '.git' in dp or 'vs2022' in dp: continue
        for f in fn:
            if f.lower().endswith(('.cpp', '.h')):
                p = os.path.join(dp, f)
                try: t = open(p, encoding='cp1251', errors='replace').read()
                except: continue
                if pat.search(t): hits.append(os.path.relpath(p, root))
    return hits
for n in cb:
    print('  %-22s recon:%-40s jan03:%s' % (n, ','.join(grep_str(rroot, n))[:40] or '-', ','.join(grep_str(jroot, n))[:40] or '-'))
