#!/usr/bin/env python
"""Build helper for the A5 VS2022 port.

usage: python b.py [-n MAXLINES] [-w WIDTH] [-f FILTER_REGEX] [--summary] target [target...]

Builds the given CMake targets (S:\\vs2022 -> C:\\a5b\\s) and prints compact, de-duplicated
diagnostics with long path prefixes stripped. Log is kept at C:\\a5b\\s\\last_<target>.log.
"""
import argparse, collections, os, re, subprocess, sys

ap = argparse.ArgumentParser()
ap.add_argument('-n', type=int, default=60)
ap.add_argument('-w', type=int, default=240)
ap.add_argument('-f', default=None, help='only show diagnostics matching this regex')
ap.add_argument('--summary', action='store_true', help='only per-file / per-code tallies')
ap.add_argument('--nobuild', action='store_true', help='re-read the last log instead of building')
ap.add_argument('targets', nargs='+')
a = ap.parse_args()

bld = os.environ.get('A5_BLD', r'C:\a5b\s')
os.makedirs(bld, exist_ok=True)
log = os.path.join(bld, 'last_%s.log' % a.targets[0])
rc = 0
if not a.nobuild:
    cmd = 'cmd /c "S:\\vs2022\\build.cmd %s RelWithDebInfo %s"' % (bld, ' '.join(a.targets))
    with open(log, 'wb') as f:
        rc = subprocess.call(cmd, stdout=f, stderr=subprocess.STDOUT, cwd='S:\\vs2022', shell=True)

text = open(log, encoding='cp1251', errors='replace').read()
subs = [
    (re.compile(r'C:\\Program Files \(x86\)\\Microsoft Visual Studio\\2022\\BuildTools\\VC\\Tools\\MSVC\\[0-9.]+\\include\\', re.I), '<msvc>/'),
    (re.compile(r'C:\\Program Files \(x86\)\\Windows Kits\\10\\include\\[0-9.]+\\', re.I), '<sdk>/'),
    (re.compile(r'S:\\[A-Za-z0-9_]+\\\.\.\\', re.I), ''),
    (re.compile(r'S:\\', re.I), ''),
]
diag = re.compile(r'(: (fatal )?error |: error LNK|LNK\d+:|^LINK : |cannot open)')
seen, out = set(), []
per_file, per_code, failed = collections.Counter(), collections.Counter(), []
for line in text.splitlines():
    if line.startswith('FAILED:'):
        m = re.search(r'S_[\\/]([A-Za-z0-9_]+)[\\/]', line)
        failed.append(m.group(1) if m else '?')
        continue
    if not diag.search(line):
        continue
    for rx, rep in subs:
        line = rx.sub(rep, line)
    if line in seen:
        continue
    seen.add(line)
    m = re.match(r'([^()]+)\(\d+(,\d+)?\)\s*: (?:fatal )?error (\w+)', line)
    if m:
        per_file[m.group(1)] += 1
        per_code[m.group(3)] += 1
    if a.f and not re.search(a.f, line):
        continue
    out.append(line)

if not a.summary:
    for line in out[:a.n]:
        print(line[:a.w])
    if len(out) > a.n:
        print('... %d more diagnostics' % (len(out) - a.n))
print('== unique errors: %d | failed objects: %d | rc=%d' % (len(seen), len(failed), rc))
if a.summary or len(out) > a.n:
    print('-- by file (top 25):', ', '.join('%s:%d' % kv for kv in per_file.most_common(25)))
    print('-- by code (top 15):', ', '.join('%s:%d' % kv for kv in per_code.most_common(15)))
    d = collections.Counter(failed)
    print('-- failed objects by project:', ', '.join('%s:%d' % kv for kv in d.most_common()))
