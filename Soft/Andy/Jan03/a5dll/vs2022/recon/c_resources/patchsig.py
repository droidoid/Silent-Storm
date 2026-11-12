"""Make Jan03-signature copies of the release packs: samples/res/*.res -> samples/packdir/*.res (copies only)."""
import os, struct
base = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'samples')
os.makedirs(os.path.join(base, 'packdir'), exist_ok=True)
for f in os.listdir(os.path.join(base, 'res')):
    data = bytearray(open(os.path.join(base, 'res', f), 'rb').read())
    if struct.unpack_from('<I', data, 0)[0] == 0x96948A22:
        struct.pack_into('<I', data, 0, 0x95938921)
        open(os.path.join(base, 'packdir', f), 'wb').write(data)
        print('patched copy', f)
