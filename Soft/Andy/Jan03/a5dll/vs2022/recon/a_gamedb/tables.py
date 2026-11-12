"""Summarise every table object (class 0xA1843130) of a release game.db and map table id -> object."""
import sys, struct, collections
from ss import load, chunks, objtable


def sub(d, s, n):
    return list(chunks(d, s, s + n))


def first(d, s, n, cid):
    for c, ps, ln in chunks(d, s, s + n):
        if c == cid:
            return ps, ln
    return None


def table_map(d, top):
    """root chunk 1 -> chunk 1 = hash_map: [1]=key, [2]=ptr alternating -> {ptr: tableid}"""
    root = [x for x in top if x[0] == 1][0]
    hm = first(d, root[1], root[2], 1)
    keys, vals = [], []
    for c, ps, ln in chunks(d, hm[0], hm[0] + hm[1]):
        if c == 1 and ln == 4:
            keys.append(struct.unpack_from('<I', d, ps)[0])
        elif c == 2 and ln == 4:
            vals.append(struct.unpack_from('<I', d, ps)[0])
    return dict(zip(vals, keys)), keys


def objects(d, top):
    """objects data chunk 2 -> {ptr: (start, len) of the object's field chunk list}"""
    res = {}
    o = [x for x in top if x[0] == 2][0]
    for c, ps, ln in chunks(d, o[1], o[1] + o[2]):
        p = first(d, ps, ln, 0)
        f = first(d, ps, ln, 1)
        ptr = struct.unpack_from('<I', d, p[0])[0]
        res[ptr] = f
    return res


def describe(d, s, n):
    """field chunks of a table object -> summary dict"""
    out = {}
    for c, ps, ln in chunks(d, s, s + n):
        items = [x for x in chunks(d, ps, ps + ln)]
        info = {'len': ln, 'items': len(items)}
        if c == 2:   # records: [1]=count of dwords, [2]=packed dwords
            counts = collections.Counter()
            for _, rs, rl in items:
                cnt = first(d, rs, rl, 1)
                counts[struct.unpack_from('<i', d, cnt[0])[0] if cnt and cnt[1] == 4 else None] += 1
            info['dwords_per_record'] = dict(counts)
        out[c] = info
    return out


if __name__ == '__main__':
    d, top = load(sys.argv[1])
    ptr2id, keys = table_map(d, top)
    objs = objects(d, top)
    print('tables in root hash:', len(keys), ' objects:', len(objs))
    for ptr, tid in sorted(ptr2id.items(), key=lambda x: x[1]):
        f = objs.get(ptr)
        desc = describe(d, *f) if f else None
        print('0x%08X %s' % (tid, desc))
