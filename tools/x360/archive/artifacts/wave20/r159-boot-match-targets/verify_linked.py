"""Read-only, pinned R159 EnableJobThread comparison; writes no files."""
import argparse
import hashlib
import json
import re
import struct
import sys
from pathlib import Path

import pefile

sys.path.insert(0, 'D:/Reverse/BP-Decomp_Workflow/tools/re')
import x360rd

FUNC = '?EnableJobThread@HardwareInit@CgsSystem@@SAXXZ'
CALLS = {12: '?BeginAssert@Assert@CgsDev@@YAXXZ',
         36: '?FireAssert@Assert@CgsDev@@YAXPBD0H@Z',
         40: '?EndAssert@Assert@CgsDev@@YAXXZ'}
PATH = b'd:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\system\\X360/CgsHardwareInitX360.cpp'
MESSAGE = b'Not supported on xbox\n'
ORIGINAL = 0x828d6138


def digest(data):
    return hashlib.sha256(data).hexdigest()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('link', type=Path)
    ap.add_argument('--head', required=True)
    ap.add_argument('--pe-sha256', required=True)
    ap.add_argument('--map-sha256', required=True)
    ap.add_argument('--seal-sha256', required=True)
    args = ap.parse_args()
    pe_data = (args.link / 'native-game.pe').read_bytes()
    map_data = (args.link / 'native-game.map').read_bytes()
    seal_data = (args.link / 'seal.json').read_bytes()
    assert digest(pe_data) == args.pe_sha256.lower()
    assert digest(map_data) == args.map_sha256.lower()
    assert digest(seal_data) == args.seal_sha256.lower()
    seal = json.loads(seal_data)
    assert seal['head'] == args.head
    result = json.loads((args.link / 'result.json').read_bytes())
    assert result['exit_code'] == 0 and result['all_post_checks_pass']
    outputs = {Path(k.replace('\\', '/')).name: v['sha256'] for k, v in result['outputs'].items()}
    assert outputs['native-game.pe'] == digest(pe_data) and outputs['native-game.map'] == digest(map_data)
    pe = pefile.PE(data=pe_data, fast_load=True)
    base = pe.OPTIONAL_HEADER.ImageBase

    def read(va, length):
        rva = va - base
        sections = [s for s in pe.sections if s.VirtualAddress <= rva and
                    rva + length <= s.VirtualAddress + min(s.Misc_VirtualSize, s.SizeOfRawData)]
        assert len(sections) == 1, hex(va)
        s = sections[0]
        start = s.PointerToRawData + rva - s.VirtualAddress
        return pe_data[start:start + length]

    def cstr(va):
        # Read one byte at a time; do not cross a section boundary.
        raw = bytearray()
        for i in range(1024):
            b = read(va + i, 1)
            if b == b'\0':
                return bytes(raw)
            raw.extend(b)
        raise ValueError('unterminated string')

    wanted = {FUNC, *CALLS.values()}
    symbols = {name: [] for name in wanted}
    rx = re.compile(r'^\s*0003:[0-9a-fA-F]{8}\s+(\S+)\s+([0-9a-fA-F]{8,16})\s+f(?:\s|$)')
    for line in map_data.decode(errors='replace').splitlines():
        match = rx.match(line)
        if match and match[1] in symbols:
            symbols[match[1]].append(int(match[2], 16))
    assert all(len(v) == 1 for v in symbols.values()), symbols
    symbols = {k: v[0] for k, v in symbols.items()}
    address = symbols[FUNC]
    pdata = next(s for s in pe.sections if s.Name.rstrip(b'\0') == b'.pdata')
    table = pe_data[pdata.PointerToRawData:pdata.PointerToRawData + pdata.Misc_VirtualSize]
    entries = list(struct.iter_unpack('>II', table))
    extents = [(a, ((f >> 8) & 0x3fffff) * 4) for a, f in entries if a == address and f >> 30 == 1]
    assert extents == [(address, 60)], extents
    old = x360rd.rd(ORIGINAL, 60)
    new = read(address, 60)
    old_words = struct.unpack('>15I', old)
    new_words = struct.unpack('>15I', new)
    masks = {12: 0xfc000003, 36: 0xfc000003, 40: 0xfc000003,
             16: 0xffff0000, 24: 0xffff0000, 28: 0xffff0000, 32: 0xffff0000}
    fixed_mismatches = [i * 4 for i, (a, b) in enumerate(zip(old_words, new_words))
                        if (a & masks.get(i * 4, 0xffffffff)) != (b & masks.get(i * 4, 0xffffffff))]

    def branch_target(word, pc):
        assert word >> 26 == 18 and word & 3 == 1
        displacement = word & 0x03fffffc
        if displacement & 0x02000000:
            displacement -= 0x04000000
        return (pc + displacement) & 0xffffffff

    assert [branch_target(old_words[o // 4], ORIGINAL + o) for o in CALLS] == [
        0x82817548, 0x82820810, 0x82817558]
    assert old_words[5] == 0x38a000f7
    assert x360rd.rd(0x820f7938, len(PATH) + 1) == PATH + b'\0'
    assert x360rd.rd(0x820f7920, len(MESSAGE) + 1) == MESSAGE + b'\0'
    call_checks = {name: branch_target(new_words[offset // 4], address + offset) == symbols[name]
                   for offset, name in CALLS.items()}
    def addr(high_offset, low_offset):
        hi, lo = new_words[high_offset // 4], new_words[low_offset // 4]
        return (((hi & 0xffff) << 16) + struct.unpack('>h', struct.pack('>H', lo & 0xffff))[0]) & 0xffffffff
    path, message = cstr(addr(16, 24)), cstr(addr(28, 32))
    line = new_words[5] & 0xffff
    checks = {'fixed_instruction_bits': not fixed_mismatches, 'line_247': new_words[5] == 0x38a000f7,
              'path': path == PATH, 'message': message == MESSAGE, 'named_call_targets': all(call_checks.values())}
    print(json.dumps({'head': seal['head'], 'pe_sha256': digest(pe_data), 'map_sha256': digest(map_data),
                      'seal_sha256': digest(seal_data), 'artist_address': hex(ORIGINAL),
                      'linked_address': hex(address), 'bytes': 60, 'raw_exact': old == new,
                      'verified_address_equivalence': all(checks.values()), 'checks': checks,
                      'line': line, 'path': path.decode(errors='replace'),
                      'message': message.decode(errors='replace'), 'calls': call_checks,
                      'fixed_mismatch_offsets': [hex(x) for x in fixed_mismatches]}, indent=2))


if __name__ == '__main__':
    main()
