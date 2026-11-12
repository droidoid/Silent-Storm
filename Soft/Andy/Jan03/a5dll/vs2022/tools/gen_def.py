#!/usr/bin/env python
"""Generate a module-definition (.def) file for a DLL from its object files.

usage: python gen_def.py --out X.def --name Main  --provider @objs.rsp --consumer @objs.rsp [--consumer ...]
       python gen_def.py --out X.def --name Input --provider @objs.rsp --all

Default mode (Main): exports = symbols defined (external) in the provider's objects that are referenced but
not defined by the consumers' objects. This replaces the tool-generated ReleaseDll.def of the original build
(VC7/STLport manglings, unusable today) and stays under the 65535-export limit of the PE format, which
"export everything" exceeds for Main.

--all (the small DLLs): export every external symbol the provider's objects define, with the rules of CMake's
WINDOWS_EXPORT_ALL_SYMBOLS (Source/bindexplib.cxx): no scalar/vector deleting destructors (??_G, ??_E), no
names with '.', writable data -> DATA, read-only constants (string literals, FP constants, EH tables, RTTI
other than vftables) are left out, vftables (??_7) are exported as code. It replaces "cmake -E __create_def",
whose result depends on the CMake version: 3.29 wrote a broken line "=" for objects of a newer compiler, which
failed the link of Input.dll with LNK2001. Here the list does not depend on CMake at all.

The objects are read directly (COFF, regular and /bigobj); no dumpbin needed. x86 only (leading '_' of C
names is removed, as the linker adds it back for an x86 .def).
"""
import argparse, re, struct, sys

ap = argparse.ArgumentParser()
ap.add_argument('--out', required=True)
ap.add_argument('--name', required=True)
ap.add_argument('--provider', action='append', default=[])
ap.add_argument('--consumer', action='append', default=[])
ap.add_argument('--all', action='store_true', help='export everything the provider defines (CMake rules)')
a = ap.parse_args()


def expand(items):
    out = []
    for it in items:
        if it.startswith('@'):
            out += open(it[1:]).read().replace(';', '\n').split('\n')
        else:
            out.append(it)
    return [x.strip().strip('"') for x in out if x.strip()]


IMAGE_FILE_MACHINE_I386 = 0x14C
IMAGE_SYM_CLASS_EXTERNAL = 2
SCN_EXECUTE, SCN_READ, SCN_WRITE = 0x20000000, 0x40000000, 0x80000000
BIGOBJ_CLSID = bytes.fromhex('c7a1bad1eebaa94baf20faf66aa4dcb8')


def read_obj(path):
    """-> (defined {name: (is_function_type, section_characteristics)}, undefined {name})"""
    d = open(path, 'rb').read()
    if d[0:4] == b'\x00\x00\xff\xff' and d[12:28] == BIGOBJ_CLSID:      # /bigobj: ANON_OBJECT_HEADER_BIGOBJ
        machine, = struct.unpack_from('<H', d, 6)
        nsect, symptr, nsym = struct.unpack_from('<III', d, 44)
        sect_off, sym_size, sect_fmt = 56, 20, '<8sIiHBB'
    else:
        machine, nsect, _, symptr, nsym, opt, _ = struct.unpack_from('<HHIIIHH', d, 0)
        sect_off, sym_size, sect_fmt = 20 + opt, 18, '<8sIhHBB'
        if machine == 0 and nsect == 0xFFFF:
            sys.exit('gen_def: %s: unsupported object format (LTCG /GL?)' % path)
    if machine != IMAGE_FILE_MACHINE_I386:
        sys.exit('gen_def: %s: not an x86 object (machine 0x%X)' % (path, machine))
    chars = [struct.unpack_from('<I', d, sect_off + 40 * i + 36)[0] for i in range(nsect)]
    strtab = symptr + nsym * sym_size
    defined, undefined = {}, set()
    i = 0
    while i < nsym:
        off = symptr + i * sym_size
        raw, value, sect, typ, cls, naux = struct.unpack_from(sect_fmt, d, off)
        i += 1 + naux
        if cls != IMAGE_SYM_CLASS_EXTERNAL:
            continue
        if raw[:4] == b'\0\0\0\0':
            p = strtab + struct.unpack_from('<I', raw, 4)[0]
            name = d[p:d.index(b'\0', p)]
        else:
            name = raw.rstrip(b'\0')
        name = name.decode('latin-1')
        if sect == 0:
            if value == 0:              # value != 0: common (tentative) definition
                undefined.add(name)
            continue
        if sect > 0:
            defined[name] = ((typ & 0x30) == 0x20, chars[sect - 1])
    return defined, undefined


def scan(objs):
    defined, undefined = {}, set()
    for o in objs:
        if o.lower().endswith('.res'):      # compiled resources travel in the object list too
            continue
        dd, uu = read_obj(o)
        defined.update(dd)
        undefined |= uu
    return defined, undefined


C_NAME = re.compile(r'^[A-Za-z_$][A-Za-z0-9_$]*(@[0-9]+)?$')


def export_all(defined):
    """CMake bindexplib rules -> {name: is_data}"""
    out = {}
    for sym, (is_func, ch) in defined.items():
        if sym.startswith('??_G') or sym.startswith('??_E') or '.' in sym or sym.startswith('__imp_'):
            continue
        # Undecorated names must be identifiers. guiddef.h (Windows SDK) gives the GUID operator== C linkage,
        # i.e. a symbol "_==" in every object that includes dinput.h; CMake 3.29 wrote it as a line "=".
        if not sym.startswith(('?', '@')) and not C_NAME.match(sym):
            continue
        if not is_func and (ch & SCN_WRITE):
            out[sym] = True
        elif is_func or not (ch & SCN_READ) or (ch & SCN_EXECUTE) or sym.startswith('??_7'):
            out[sym] = False
    return out


prov_objs = expand(a.provider)
if not prov_objs:
    sys.exit('gen_def: %s - no provider objects' % a.name)
prov_def, _ = scan(prov_objs)

if a.all:
    exports = export_all(prov_def)
else:
    cons_def, cons_undef = scan(expand(a.consumer))
    exports = {}
    for name in cons_undef:
        via_imp = name.startswith('__imp_')
        real = name[6:] if via_imp else name
        if real in cons_def or real not in prov_def:
            continue
        is_func = prov_def[real][0]
        exports[real] = (not is_func) and via_imp


def export_name(sym):
    # C++ decorated and __fastcall names go in verbatim; C names lose the leading '_' and a __stdcall '@N'
    # suffix (the x86 linker matches the undecorated name, as in CMake's .def files).
    if sym.startswith('?') or sym.startswith('@'):
        return sym
    if sym.startswith('_'):
        sym = sym[1:]
        if '@' in sym and sym.rsplit('@', 1)[1].isdigit():
            sym = sym.rsplit('@', 1)[0]
    return sym


if not exports:
    sys.exit('gen_def: %s - empty export list' % a.name)
lines = ['\t%s%s\n' % (export_name(s), ' DATA' if exports[s] else '') for s in sorted(exports)]
bad = [l for l in lines if not l.strip() or l.strip().split()[0] in ('=', 'DATA')]
if bad:
    sys.exit('gen_def: %s - malformed export lines: %r' % (a.name, bad[:5]))
text = '; generated by vs2022/tools/gen_def.py - do not edit\nLIBRARY %s\nEXPORTS\n%s' % (a.name, ''.join(lines))
try:
    unchanged = open(a.out).read() == text
except OSError:
    unchanged = False
if not unchanged:
    with open(a.out, 'w', newline='\n') as f:
        f.write(text)
print('gen_def: %s exports %d symbols (%d data)' % (a.name, len(exports), sum(exports.values())))
