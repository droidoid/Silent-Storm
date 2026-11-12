"""Complete\cfg\*.cfg vs Jan03: console commands (registry), setvar variables, bind targets.
Known in Jan03 = name appears as a string literal "name" in Jan03 sources (registration, GetVar, CVar/CCmd, CBind, event observers).
Release check = name appears as NUL-terminated ASCII string in the release exe.
usage: python cfg_check.py <Complete\cfg> <Jan03 a5dll> <release exe> > cfg_check.txt"""
import sys, os, re, glob, collections

cfgdir, src, exe = sys.argv[1:4]
lits = set()
regs = set()
for fn in glob.glob(os.path.join(src, '**', '*.cpp'), recursive=True) + glob.glob(os.path.join(src, '**', '*.h'), recursive=True):
    if os.sep + 'vs2022' + os.sep in fn:
        continue
    t = open(fn, encoding='cp1251', errors='replace').read()
    t = re.sub(r'//[^\n]*', '', t)
    lits |= set(re.findall(r'L?"([A-Za-z_+!][\w+]*)"', t))
    regs |= set(re.findall(r'REGISTER_(?:CMD|VAR|VAR_EX)\(\s*"([^"]+)"', t))
    regs |= set(re.findall(r'Register(?:Cmd|Var)\(\s*"([^"]+)"', t))
_eb = open(exe, 'rb').read()
exe_s = set(m.decode('latin-1') for m in re.findall(rb'(?<=\x00)([A-Za-z_+!][\w+]{1,60})(?=\x00)', _eb))
exe_s |= set(m.decode('utf-16le') for m in re.findall(rb'((?:[A-Za-z_]\x00)(?:[\w]\x00){1,60})(?=\x00\x00)', _eb))
cmds, vars_, binds = collections.Counter(), collections.Counter(), collections.Counter()
for fn in glob.glob(os.path.join(cfgdir, '*.cfg')):
    for ln in open(fn, encoding='cp1251'):
        ln = ln.strip()
        if not ln or ln.startswith('//'):
            continue
        w = ln.split()
        cmds[w[0]] += 1
        if w[0] == 'setvar' and len(w) > 1:
            vars_[w[1]] += 1
        if w[0] == 'bind' and len(w) > 1:
            binds[w[1].lstrip('+!-')] += 1


def rep(title, c, known):
    k = [n for n in c if n in known]
    u = [n for n in c if n not in known]
    ur = [n for n in u if n in exe_s]
    print('== %s: %d distinct, known to Jan03 %d, unknown %d (of these present in release exe: %d)' % (title, len(c), len(k), len(u), len(ur)))
    for n in sorted(u, key=lambda x: -c[x]):
        print('   %-32s uses=%d  in_release_exe=%s' % (n, c[n], n in exe_s))


rep('top-level console commands (registry)', cmds, regs)
rep('setvar variables (literal in Jan03 source)', vars_, lits | regs)
rep('bind targets (literal in Jan03 source)', binds, lits | regs)
print('Jan03 registered console names (REGISTER_*/Register*):', len(regs))

