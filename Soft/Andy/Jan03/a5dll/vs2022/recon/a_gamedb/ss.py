"""CStructureSaver reader following Soft/Andy/Jan03/a5dll/FileIO/BasicChunk1.cpp (Jan03).
File = top-level chunks: 0 = object table (typeID:4, ptr:4, valid:1)*, 1 = main data, 2 = objects data
(chunks id 1 numbered per object: {chunk 0 = ptr, chunk 1 = object fields})."""
import struct, sys, collections


def chunks(buf, start=0, end=None):
    """yield (id, payload_start, payload_len) of the chunk list in buf[start:end]"""
    end = len(buf) if end is None else end
    pos = start
    while pos < end:
        cid = buf[pos]
        b = buf[pos + 1]
        if b & 1:
            ln = struct.unpack_from('<I', buf, pos + 1)[0] >> 1
            hdr = 5
        else:
            ln = b >> 1
            hdr = 2
        if pos + hdr + ln > end:
            raise ValueError('chunk overrun at %d (id %d len %d end %d)' % (pos, cid, ln, end))
        yield cid, pos + hdr, ln
        pos += hdr + ln


def is_chunk_list(buf, s, n):
    try:
        tot = 0
        for cid, ps, ln in chunks(buf, s, s + n):
            tot += 1
        return tot > 0
    except Exception:
        return False


def load(path):
    d = open(path, 'rb').read()
    top = list(chunks(d))
    return d, top


def objtable(d, top):
    out = []
    for cid, s, n in top:
        if cid == 0:
            for i in range(0, n - n % 9, 9):
                t, p, v = struct.unpack_from('<IIB', d, s + i)
                out.append((t, p, v))
    return out


if __name__ == '__main__':
    d, top = load(sys.argv[1])
    print(sys.argv[1], len(d), 'bytes; top-level chunks:', [(c, n) for c, _, n in top])
    ot = objtable(d, top)
    hist = collections.Counter(t for t, _, _ in ot)
    print('objects:', len(ot), 'distinct class ids:', len(hist))
    for t, c in hist.most_common(40):
        print('  0x%08X  %d' % (t, c))
