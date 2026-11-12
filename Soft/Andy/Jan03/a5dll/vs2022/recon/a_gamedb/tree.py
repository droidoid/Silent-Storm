import sys, struct
from ss import load, chunks, is_chunk_list


def show(d, s, n, depth, maxdepth, maxitems, indent=''):
    k = 0
    for cid, ps, ln in chunks(d, s, s + n):
        k += 1
        if k > maxitems:
            print(indent + '  ...')
            break
        sub = depth < maxdepth and ln >= 2 and is_chunk_list(d, ps, ln)
        raw = d[ps:ps + min(ln, 24)]
        extra = ''
        if ln == 4:
            extra = ' int=%d (0x%08X)' % (struct.unpack_from('<i', d, ps)[0], struct.unpack_from('<I', d, ps)[0])
        print('%s[%d] len=%d%s  %s' % (indent, cid, ln, extra, '' if sub else raw.hex(' ')))
        if sub:
            show(d, ps, ln, depth + 1, maxdepth, maxitems, indent + '    ')


if __name__ == '__main__':
    d, top = load(sys.argv[1])
    which = int(sys.argv[2])
    maxdepth = int(sys.argv[3]) if len(sys.argv) > 3 else 3
    maxitems = int(sys.argv[4]) if len(sys.argv) > 4 else 12
    for cid, s, n in top:
        if cid == which:
            print('top chunk', cid, 'len', n, d[s:s + 16].hex(' '))
            show(d, s, n, 0, maxdepth, maxitems)
