import pathlib
import struct
import sys
from capstone import Cs, CS_ARCH_PPC, CS_MODE_32, CS_MODE_BIG_ENDIAN

md = Cs(CS_ARCH_PPC, CS_MODE_32 | CS_MODE_BIG_ENDIAN)

def inspect(path, owner):
    data = pathlib.Path(path).read_bytes()
    machine, nsec, timestamp, symptr, nsym, optsz, flags = struct.unpack_from('<HHIIIHH', data)
    strtab = symptr + nsym * 18
    def decode_name(raw):
        if raw[:4] == b'\0\0\0\0':
            off = struct.unpack_from('<I', raw, 4)[0]
            return data[strtab + off:].split(b'\0', 1)[0].decode(errors='replace')
        if raw.startswith(b'/'):
            off = int(raw[1:].split(b'\0', 1)[0])
            return data[strtab + off:].split(b'\0', 1)[0].decode(errors='replace')
        return raw.split(b'\0', 1)[0].decode(errors='replace')
    sections = []
    for idx in range(nsec):
        off = 20 + optsz + 40 * idx
        raw = data[off:off + 40]
        name = decode_name(raw[:8])
        vsize, vaddr, size, rawptr, relptr, lineptr, nrel, nline, chars = struct.unpack_from('<IIIIIIHHI', raw, 8)
        sections.append({'idx': idx + 1, 'name': name, 'size': size, 'raw': rawptr, 'rel': relptr, 'nrel': nrel})
    symbols = {}
    i = 0
    while i < nsym:
        off = symptr + 18 * i
        name = decode_name(data[off:off + 8])
        value, sec, typ, cls, aux = struct.unpack_from('<IhHBB', data, off + 8)
        symbols[i] = {'name': name, 'value': value, 'sec': sec, 'cls': cls, 'aux': aux}
        i += 1 + aux
    def reloc_list(section, low=None, high=None):
        rows = []
        for n in range(section['nrel']):
            va, symidx, typ = struct.unpack_from('<IIH', data, section['rel'] + 10 * n)
            if low is not None and not (low <= va < high):
                continue
            rows.append((va, typ, symbols.get(symidx, {}).get('name', f'#{symidx}')))
        return rows
    print(f'FILE {path} machine=0x{machine:04x} sections={nsec}')
    prefix = '?sTypeInfo@' + owner
    descriptors = [s for s in symbols.values() if s['name'].startswith(prefix)]
    for sym in descriptors:
        sec = sections[sym['sec'] - 1]
        start = sym['value']
        print(f"DESCRIPTOR {sym['name']} section={sec['name']}+0x{start:x} bytes={data[sec['raw'] + start:sec['raw'] + start + 16].hex()}")
        for va, typ, target in reloc_list(sec, start, start + 16):
            print(f'  FIELD_RELOC +0x{va-start:x} type=0x{typ:x} -> {target}')
    for sec in sections:
        if sec['name'].startswith('.CRT$XCU'):
            print(f"CRT {sec['name']} size=0x{sec['size']:x} raw={data[sec['raw']:sec['raw']+sec['size']].hex()}")
            for va, typ, target in reloc_list(sec):
                print(f'  CRT_RELOC +0x{va:x} type=0x{typ:x} -> {target}')
    want = ('?GetStaticTypeInfo@', '?GetTypeInfo@', '?GetTypeName@', '?CreateObject@', '??__E', '??0' + owner.replace('@', ''))
    for sym in symbols.values():
        n = sym['name']
        if sym['sec'] <= 0 or owner not in n:
            continue
        if not any(token in n for token in want):
            continue
        sec = sections[sym['sec'] - 1]
        start = sym['value']
        code = data[sec['raw'] + start:sec['raw'] + sec['size']]
        print(f"CODE {n} section={sec['name']}+0x{start:x} size=0x{len(code):x}")
        for insn in md.disasm(code[:min(len(code), 0x60)], start):
            print(f'  {insn.address:04x} {insn.mnemonic:8s} {insn.op_str}')
        for va, typ, target in reloc_list(sec, start, start + min(len(code), 0x60)):
            print(f'  CODE_RELOC +0x{va-start:x} type=0x{typ:x} -> {target}')

base = pathlib.Path(r'D:\Reverse\BP-X360-Experimental-Artifacts\wave20\r174-sound-descriptors-streaming-traffic\x360-compile')
inspect(base / 'BrnStreamingStateManager-e0698c27.obj', 'StreamingStateManager@Streaming@Logic@BrnSound@@')
inspect(base / 'BrnTrafficStateManager-7d00aeec.obj', 'TrafficStateManager@Traffic@Logic@BrnSound@@')
