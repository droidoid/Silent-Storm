#!/usr/bin/env python
"""Encoding-safe search/replace for the legacy sources (cp1251, CRLF).

usage: python ed.py <patchfile> [--dry]

The patch file is UTF-8 text made of blocks:

    @@ Main/Cache.h
    <<<<
    exact old text (may span lines)
    ====
    new text
    >>>>

Several <<<< ==== >>>> blocks may follow one @@ line; several @@ sections may follow each other.
Paths are relative to the a5dll root (the parent of vs2022/). The old text must occur exactly
once in the file (append "@@ path !all" to replace every occurrence). Files are read and written
as cp1251 with their original line endings, so Cyrillic comments and string literals survive.
"""
import os, sys

root = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..'))
dry = '--dry' in sys.argv
spec = open(sys.argv[1], encoding='utf-8').read().replace('\r\n', '\n')

sections, cur, mode, buf = [], None, None, []
for line in spec.split('\n'):
    if mode is None and line.startswith('@@ '):
        parts = line[3:].strip().split(' !')
        cur = {'path': parts[0].strip(), 'all': len(parts) > 1 and parts[1].strip() == 'all', 'edits': []}
        sections.append(cur)
    elif mode is None and line.strip() == '<<<<':
        mode, buf = 'old', []
    elif mode == 'old' and line.strip() == '====':
        old, mode, buf = '\n'.join(buf), 'new', []
    elif mode == 'new' and line.strip() == '>>>>':
        cur['edits'].append((old, '\n'.join(buf)))
        mode = None
    elif mode is not None:
        buf.append(line)
if mode is not None:
    sys.exit('patch file ends inside a block')

failed = 0
for s in sections:
    path = os.path.join(root, s['path'].replace('/', os.sep))
    raw = open(path, 'rb').read()
    crlf = b'\r\n' in raw
    text = raw.decode('cp1251').replace('\r\n', '\n')
    for old, new in s['edits']:
        n = text.count(old)
        if n == 0 or (n > 1 and not s['all']):
            print('FAIL %s: old text found %d times: %r' % (s['path'], n, old[:70]))
            failed += 1
            continue
        text = text.replace(old, new)
    if not dry and not failed:
        out = text.replace('\n', '\r\n') if crlf else text
        open(path, 'wb').write(out.encode('cp1251'))
        print('ok   %s (%d edit(s))' % (s['path'], len(s['edits'])))
if failed:
    sys.exit('%d edit(s) failed; files with failures were not written' % failed)
