"""Validate the pinned R165 linked debug allocation provider and owner edges."""
import argparse
import hashlib
import json
import re
import struct
from pathlib import Path

import pefile

HEAD = '451d95889337e064441dec06a1726179187e48cf'
TREE = 'cdfbeb9f10e3b32c23e42da60c5ea72a5dcde843'
PARENT = 'd9d487ef61d4bab2e80d9e4c2fc64c20a600b08a'
NEW = '??_U@YAPAXIPAUIResourceAllocator@rw@@W4AllocationType@Internal@CgsDev@@@Z'
DELETE = '??_V@YAXPAXPAUIResourceAllocator@rw@@W4AllocationType@Internal@CgsDev@@@Z'
HELPER = '?AllocateMemoryResource@IResourceAllocator@rw@@QAAPAXIIPBD@Z'
COFF = Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave20/r165-debug-allocator-provider/CgsDebugCollections.obj')
COFF_SHA = '8bd03f1d8c1811ca0cef043eb23224f69b6f5c25536abb1753db1ef41841d875'


def sha(b):
    return hashlib.sha256(b).hexdigest()


def require(ok, message):
    if not ok:
        raise ValueError(message)


def target(word, pc):
    require(word >> 26 == 18 and word & 2 == 0, 'not a relative PPC branch')
    offset = word & 0x03fffffc
    if offset & 0x02000000:
        offset -= 0x04000000
    return (pc + offset) & 0xffffffff


def read_coff(data):
    machine, nsec, _, offset, count, opt, _ = struct.unpack_from('<HHIIIHH', data)
    require(machine == 0x1f2, 'not PPC COFF')
    strings = data[offset + count * 18:]
    symbols = {}
    i = 0
    while i < count:
        raw, value, section, _, _, aux = struct.unpack_from('<8sIhHBB', data, offset + i * 18)
        str_off = struct.unpack_from('<I', raw, 4)[0] if raw[:4] == bytes(4) else 0
        name = (strings[str_off:strings.find(b'\0', str_off)] if str_off else raw.rstrip(b'\0')).decode('latin1')
        symbols[i] = (name, value, section)
        i += 1 + aux
    def section(name):
        v = [x for x in symbols.values() if x[0] == name and x[2] > 0]
        require(len(v) == 1 and v[0][1] == 0, 'COFF function not unique')
        h = struct.unpack_from('<8sIIIIIIHHI', data, 20 + opt + 40 * (v[0][2] - 1))
        body = data[h[4]:h[4] + h[3]]
        rels = []
        for j in range(h[7]):
            address, idx, typ = struct.unpack_from('<IIH', data, h[5] + 10 * j)
            rels.append((address, typ, symbols[idx][0]))
        return body, rels
    return section


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--pins', type=Path, required=True)
    ap.add_argument('--pins-sha256', required=True)
    ap.add_argument('--output', type=Path, required=True)
    args = ap.parse_args()
    require(args.pins.name == 'ROOT_LINK_PINS.json' and not args.output.exists(), 'invalid pins or output')
    p = args.pins.read_bytes()
    require(sha(p) == args.pins_sha256.lower(), 'pins hash mismatch')
    pins = json.loads(p)
    require((pins['head'], pins['tree'], pins['parent']) == (HEAD, TREE, PARENT), 'wrong integration identity')
    blobs = {}
    for key in ('pe', 'map', 'seal', 'completion', 'result'):
        row = pins['pins'][key]
        b = Path(row['path']).read_bytes()
        require(sha(b) == row['sha256'], key + ' pin mismatch')
        blobs[key] = b
    seal, completion, result = (json.loads(blobs[k]) for k in ('seal', 'completion', 'result'))
    require(seal['head'] == HEAD and completion['head'] == HEAD and completion['status'] == 'SEALED_NO_REGRESSIONS' and
            completion['requested'] == completion['passed'] and completion['failed'] == 0 and
            result['exit_code'] == 0 and result['all_post_checks_pass'], 'invalid seal or completion')
    result_outputs = {Path(k.replace('\\', '/')).name: v['sha256'] for k, v in result['outputs'].items()}
    require(result_outputs.get('native-game.pe') == sha(blobs['pe']) and
            result_outputs.get('native-game.map') == sha(blobs['map']), 'result pin mismatch')
    data = COFF.read_bytes()
    require(sha(data) == COFF_SHA, 'donor COFF hash mismatch')
    getcoff = read_coff(data)
    provider, prels = getcoff(NEW)
    helper, hrels = getcoff(HELPER)
    require(len(provider) == 24 and len(helper) == 152 and prels == [(20, 6, HELPER)], 'unexpected donor shape')
    require({(a, n) for a, _, n in hrels} == {(4, '__savegprlr_26'), (40, '??0BaseResourceDescriptor@rw@@QAA@XZ'),
                                                (148, '__restgprlr_26')}, 'unexpected helper relocations')
    rx = re.compile(r'^\s*0003:[0-9a-fA-F]{8}\s+(\S+)\s+([0-9a-fA-F]{8,16})\s+f\s+(?:i\s+)?(\S+)')
    rows = []
    all_symbols = []
    for line in blobs['map'].decode(errors='replace').splitlines():
        label = re.match(r'^\s*0003:[0-9a-fA-F]{8}\s+(\S+)\s+([0-9a-fA-F]{8,16})(?:\s|$)', line)
        if label:
            all_symbols.append((label[1], int(label[2], 16)))
        m = rx.match(line)
        if m:
            rows.append((m[1], int(m[2], 16), m[3]))
    def addr(name):
        addresses = {va for n, va in all_symbols if n == name}
        require(len(addresses) == 1, 'ambiguous/missing map symbol: ' + name)
        return next(iter(addresses))
    new_va = addr(NEW)
    require(any(n == NEW and owner.startswith('CgsDebugCollections-') for n, _, owner in rows), 'wrong provider owner')
    require(not any(n == DELETE for n, _, _ in rows), 'placement delete is linked')
    pe = pefile.PE(data=blobs['pe'], fast_load=True)
    base = pe.OPTIONAL_HEADER.ImageBase
    sec = {s.Name.rstrip(b'\0'): s for s in pe.sections}
    text, pdata = sec[b'.text'], sec[b'.pdata']
    def code(va, size):
        off = va - base - text.VirtualAddress
        require(0 <= off <= off + size <= min(text.SizeOfRawData, text.Misc_VirtualSize), 'outside code')
        return blobs['pe'][text.PointerToRawData + off:text.PointerToRawData + off + size]
    distinct = sorted({v for _, v, _ in rows})
    def verify_object(body, rels, va):
        next_sym = next((a for a in distinct if a > va), base + text.VirtualAddress + text.Misc_VirtualSize)
        require(next_sym >= va + len(body), 'intervening map symbol')
        linked = code(va, len(body))
        mask_offsets = {a: (t, n) for a, t, n in rels}
        for i in range(0, len(body), 4):
            old, new = struct.unpack_from('>I', body, i)[0], struct.unpack_from('>I', linked, i)[0]
            if i in mask_offsets:
                typ, name = mask_offsets[i]
                require(typ == 6 and old & 0xfc000003 == new & 0xfc000003, 'branch opcode mismatch')
                require(target(new, va + i) == addr(name), 'wrong relocation target: ' + name)
            else:
                require(old == new, 'fixed word mismatch at ' + hex(i))
        return linked
    linked_new = verify_object(provider, prels, new_va)
    helper_va = target(struct.unpack_from('>I', linked_new, 20)[0], new_va + 20)
    require(helper_va == addr(HELPER), 'provider helper mismatch')
    verify_object(helper, hrels, helper_va)
    for off, word in {0x40:0x39400005, 0x44:0x39000000, 0x48:0x39200001, 0x50:0x910bfffc,
                      0x54:0x912b0000, 0x6c:0x93810070, 0x74:0x93610074, 0x80:0x816b0010,
                      0x88:0x4e800421, 0x8c:0x80610050}.items():
        require(struct.unpack_from('>I', helper, off)[0] == word, 'descriptor proof word mismatch')
    entries = list(struct.iter_unpack('>II', blobs['pe'][pdata.PointerToRawData:pdata.PointerToRawData + pdata.Misc_VirtualSize]))
    sizes = {}
    for i, (va, flags) in enumerate(entries):
        size = ((flags >> 8) & 0x3fffff) * 4
        end = entries[i + 1][0] if i + 1 < len(entries) else base + text.VirtualAddress + text.Misc_VirtualSize
        if flags >> 30 == 1 and size and base + text.VirtualAddress <= va < va + size <= end:
            sizes[va] = size
    prefixes = ('CgsLogWindow-', 'CgsFunctionManager-', 'CgsMenuManager-', 'CgsVariableManager-')
    witnesses = {p: [] for p in prefixes}
    for name, va, owner in rows:
        prefix = next((p for p in prefixes if owner.startswith(p)), None)
        if not prefix or va not in sizes:
            continue
        body = code(va, sizes[va])
        for off in range(0, len(body), 4):
            word = struct.unpack_from('>I', body, off)[0]
            if word >> 26 == 18 and word & 3 == 1 and target(word, va + off) == new_va:
                witnesses[prefix].append({'function': name, 'address': hex(va + off)})
    require(all(witnesses.values()), 'missing owner allocation call: ' + str(witnesses))
    report = {'status':'pass', 'head':HEAD, 'tree':TREE, 'pins_sha256':sha(p), 'pe_sha256':sha(blobs['pe']),
              'map_sha256':sha(blobs['map']), 'provider':hex(new_va), 'provider_bytes':len(provider),
              'helper':hex(helper_va), 'helper_bytes':len(helper), 'linked_coff_bits_match_with_verified_branches':True,
              'lanes':[['size',16],[0,1],[0,1],[0,1],[0,1]], 'name':None, 'virtual_slot':'0x10',
              'return':'lane zero', 'placement_delete_symbol_present':False, 'owner_direct_call_witnesses':witnesses,
              'scope':'static linked PE, donor COFF and map; no teardown or runtime claim'}
    with args.output.open('x', encoding='utf-8') as f:
        json.dump(report, f, indent=2)
        f.write('\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
