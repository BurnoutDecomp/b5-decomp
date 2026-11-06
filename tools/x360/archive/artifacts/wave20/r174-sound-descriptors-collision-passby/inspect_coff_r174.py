import pathlib
import struct
import sys


def inspect(path, descriptor_prefix, type_info_prefix, static_getter_prefix, instance_getter_prefix, type_name_getter_prefix, initializer_prefix):
    data = pathlib.Path(path).read_bytes()
    machine, nsec, _, symptr, nsym, optlen, _ = struct.unpack_from('<HHIIIHH', data, 0)
    strptr = symptr + nsym * 18
    strings = data[strptr:]

    def cstr(buf, offset=0):
        return buf[offset:].split(b'\0', 1)[0].decode('utf-8', 'replace')

    def coff_name(raw):
        if raw[:4] == b'\0\0\0\0':
            return cstr(strings, struct.unpack_from('<I', raw, 4)[0])
        if raw.startswith(b'/'):
            return cstr(strings, int(raw[1:].split(b'\0', 1)[0]))
        return cstr(raw)

    sections = {}
    for i in range(nsec):
        off = 20 + optlen + i * 40
        raw = data[off:off + 40]
        name = coff_name(raw[:8])
        vsz, va, size, rawptr, relptr, _, nrel, _, flags = struct.unpack_from('<IIIIIIHHI', raw, 8)
        relocs = []
        for j in range(nrel):
            rva, symidx, rtype = struct.unpack_from('<IIH', data, relptr + j * 10)
            relocs.append((rva, symidx, rtype))
        sections[i + 1] = dict(name=name, size=size, rawptr=rawptr, relocs=relocs, flags=flags)

    symbols = {}
    i = 0
    while i < nsym:
        off = symptr + i * 18
        raw = data[off:off + 18]
        name = coff_name(raw[:8])
        value, secnum, typ, storage, aux = struct.unpack_from('<IhHBB', raw, 8)
        symbols[i] = dict(name=name, value=value, secnum=secnum, storage=storage, aux=aux)
        i += 1 + aux

    def relocation_record(symbol, lo, hi):
        sec = sections[symbol['secnum']]
        found = []
        for rva, si, rtype in sec['relocs']:
            if lo <= rva < hi:
                target = symbols.get(si, {}).get('name', f'<symbol-index-{si}>')
                found.append((rva - lo, rtype, target))
        return found

    print(f'OBJECT {path}')
    print(f'MACHINE 0x{machine:04X} sections={nsec}')
    for sym in symbols.values():
        n = sym['name']
        if sym['secnum'] > 0 and n.startswith(descriptor_prefix):
            sec = sections[sym['secnum']]
            offset = sec['rawptr'] + sym['value']
            blob = data[offset:offset + 16]
            print(f'DESCRIPTOR symbol={n} section={sec["name"]} value=0x{sym["value"]:X} data={blob.hex()}')
            for roff, rtype, target in relocation_record(sym, sym['value'], sym['value'] + 16):
                print(f'  ADDR_RELOC +0x{roff:X} type=0x{rtype:04X} target={target}')
        if sym['secnum'] > 0 and (n.startswith(type_info_prefix) or n.startswith(static_getter_prefix) or n.startswith(instance_getter_prefix) or n.startswith(type_name_getter_prefix) or n.startswith(initializer_prefix)):
            sec = sections[sym['secnum']]
            print(f'CODE symbol={n} section={sec["name"]} value=0x{sym["value"]:X}')
            for roff, rtype, target in relocation_record(sym, sym['value'], sym['value'] + 16):
                print(f'  CODE_RELOC +0x{roff:X} type=0x{rtype:04X} target={target}')
    for sec in sections.values():
        if sec['name'] == '.CRT$XCU':
            raw = data[sec['rawptr']:sec['rawptr'] + sec['size']]
            print(f'CRT_XCU size={sec["size"]} raw={raw.hex()}')
            for rva, si, rtype in sec['relocs']:
                target = symbols.get(si, {}).get('name', f'<symbol-index-{si}>')
                print(f'  CRT_RELOC +0x{rva:X} type=0x{rtype:04X} target={target}')
    print()


inspect(sys.argv[1], '?sTypeInfo@CollisionStateManager', '?sTypeInfo@CollisionStateManager', '?GetStaticTypeInfo@CollisionStateManager', '?GetTypeInfo@CollisionStateManager', '?GetTypeName@CollisionStateManager', '??__E')
inspect(sys.argv[2], '?sTypeInfo@PassbyStateManager', '?sTypeInfo@PassbyStateManager', '?GetStaticTypeInfo@PassbyStateManager', '?GetTypeInfo@PassbyStateManager', '?GetTypeName@PassbyStateManager', '??__E')


