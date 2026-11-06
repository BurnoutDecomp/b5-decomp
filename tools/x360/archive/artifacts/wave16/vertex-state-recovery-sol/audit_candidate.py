import hashlib
import json
from pathlib import Path
import re
import struct
import sys

sys.path.insert(0, 'D:/Reverse/BP-Decomp_Workflow/tools/re')
import x360rd

ARTIFACT = Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave16/vertex-state-recovery-sol')
WORKTREE = Path('D:/Reverse/BP-X360-Experimental-Worktrees/cxx03_vec2_five_luna_04ed')
GATE = ARTIFACT / 'gate2'
assert x360rd.u32(0x82815570) == 0x7c8a2378
assert x360rd.u32(0x82F264A8) == 4


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


class Coff:
    def __init__(self, path):
        self.path = Path(path)
        self.data = self.path.read_bytes()
        self.machine, nsection, _, symptr, nsym, optsize, _ = struct.unpack_from('<HHIIIHH', self.data)
        assert self.machine == 0x1f2
        strings = self.data[symptr + nsym * 18:]
        self.symbols = {}
        self.byindex = {}
        i = 0
        while i < nsym:
            offset = symptr + i * 18
            rawname = self.data[offset:offset + 8]
            if rawname[:4] == b'\0' * 4:
                nameoff = struct.unpack_from('<I', rawname, 4)[0]
                name = strings[nameoff:strings.index(b'\0', nameoff)].decode('utf-8', 'replace')
            else:
                name = rawname.split(b'\0', 1)[0].decode('utf-8', 'replace')
            value, section, typ, storage, naux = struct.unpack_from('<IhHBB', self.data, offset + 8)
            row = {'name': name, 'value': value, 'section': section, 'type': typ,
                   'storage': storage, 'naux': naux, 'symbol_index': i}
            self.symbols.setdefault(name, []).append(row)
            self.byindex[i] = row
            i += 1 + naux
        self.sections = {}
        for index in range(1, nsection + 1):
            offset = 20 + optsize + (index - 1) * 40
            name, _, _, size, rawptr, relptr, _, nreloc, _, flags = struct.unpack_from('<8sIIIIIIHHI', self.data, offset)
            relocs = []
            for j in range(nreloc):
                at, symindex, typ = struct.unpack_from('<IIH', self.data, relptr + 10 * j)
                relocs.append({'offset': at, 'type': typ,
                               'symbol': self.byindex.get(symindex, {}).get('name'),
                               'symbol_index': symindex})
            self.sections[index] = {'name': name.split(b'\0', 1)[0].decode(),
                                    'size': size, 'rawptr': rawptr,
                                    'flags': flags, 'relocs': relocs}

    def find(self, prefix):
        found = [row for rows in self.symbols.values() for row in rows
                 if row['name'].startswith(prefix) and row['section'] > 0]
        assert len(found) == 1, (prefix, found)
        return found[0]

    def function(self, prefix, length):
        symbol = self.find(prefix)
        section = self.sections[symbol['section']]
        start = section['rawptr'] + symbol['value']
        code = self.data[start:start + length]
        return symbol, section, code


objects = {p.stem.split('-')[0]: Coff(p) for p in GATE.glob('*.obj')}
comparisons = []
for object_key, prefix, address, length in [
    ('vertexprogramstate', '?InitializeNoBindX360@VertexProgramState@', 0x82b629c0, 24),
    ('vertexprogramstate', '?GetD3DVertexShader@VertexProgramState@', 0x82b60b88, 12),
    ('indexbuffer', '?GetParameters@IndexBuffer@', 0x82b60730, 48),
]:
    obj = objects[object_key]
    symbol, section, code = obj.function(prefix, length)
    original = x360rd.rd(address, length)
    assert not section['relocs'], (prefix, section['relocs'])
    assert code == original, (prefix, code.hex(), original.hex())
    comparisons.append({'symbol': symbol['name'], 'object': str(obj.path),
                        'object_sha256': sha(obj.path), 'section': symbol['section'],
                        'section_size': section['size'], 'function_offset': symbol['value'],
                        'target_va': hex(address), 'length': length, 'instructions': length // 4,
                        'compiled_hex': code.hex(), 'target_hex': original.hex(),
                        'relocations': [], 'exact_bytes_equal': True})

shadow = objects['shadowingdevice']
sym, sec, code = shadow.function('?Initialize@Device@shadow@@', 0x74)
original = x360rd.rd(0x827ed6e0, 0x74)
compiled_words = list(struct.unpack('>29I', code))
original_words = list(struct.unpack('>29I', original))
relocation_offsets = {r['offset'] - sym['value'] for r in sec['relocs']}
assert relocation_offsets == {0x0c, 0x18, 0x1c, 0x20, 0x24}, relocation_offsets
masked = []
for i, (compiled, target) in enumerate(zip(compiled_words, original_words)):
    offset = i * 4
    mask = 0xfc000003 if offset == 0x0c else 0xffff0000 if offset in relocation_offsets else 0xffffffff
    assert (compiled & mask) == (target & mask), (offset, hex(compiled), hex(target), hex(mask))
    masked.append({'offset': hex(offset), 'compiled': hex(compiled), 'target': hex(target), 'mask': hex(mask)})
initialize = {'symbol': sym['name'], 'target_va': '0x827ed6e0', 'length': 0x74,
              'all_non_relocated_bits_equal': True, 'relocations': sec['relocs'],
              'words': masked, 'meaning': 'construct and copy all five Resource words; preserve Device::Initialize return'}

ctor_symbol, ctor_section, ctor_code = shadow.function('??__EsVertexProgramStateResource@shadow@@', 36)
target_ctor = x360rd.rd(0x82c6c530, 40)
ctor_compiled_words = list(struct.unpack('>9I', ctor_code))
ctor_target_words = list(struct.unpack('>10I', target_ctor))
assert ctor_compiled_words[3:8] == ctor_target_words[3:8]
assert ctor_compiled_words[-1] == ctor_target_words[-1] == 0x4e800020
assert ctor_target_words[8] == 0x914b0000
ctor = {'symbol': ctor_symbol['name'], 'compiled_length': 36, 'target_va': '0x82c6c530',
        'target_length': 40, 'compiled_hex': ctor_code.hex(), 'target_hex': target_ctor.hex(),
        'all_five_zero_stores_equal': True,
        'target_only_instruction': 'stw r10,0(r11) repeats already-zero lane0 immediately before blr',
        'not_exact_byte_match': True, 'relocations': ctor_section['relocs']}
crt_sections = [{'index': i, **s} for i, s in shadow.sections.items() if s['name'] == '.CRT$XC']
# COFF short section names use a string-table offset for .CRT$XCU in some compiler versions.
crt_refs = [{'index': i, 'name': s['name'], 'relocations': s['relocs']}
            for i, s in shadow.sections.items()
            if any(r['symbol'] == ctor_symbol['name'] for r in s['relocs'])]
assert crt_refs, 'Resource dynamic initializer has no CRT relocation'

bindings = []
report = json.loads((GATE / 'compile-results.json').read_text())
freeze = json.loads((ARTIFACT / 'FROZEN_CONTEXT_BEFORE_gate2.json').read_text())
assert json.loads((ARTIFACT / 'FROZEN_CONTEXT_COMPARISON_gate2.json').read_text())['changed'] == {}
for row in report['results']:
    log = Path(row['log']).read_text(encoding='utf-8')
    includes = []
    for line in log.splitlines():
        if 'including file:' not in line.lower():
            continue
        text = re.split('including file:', line, maxsplit=1, flags=re.IGNORECASE)[1].strip()
        p = Path(text).resolve()
        digest = sha(p)
        assert freeze.get(str(p)) == digest, ('include outside frozen context', str(p))
        assert not any(part.lower() == 'pc' for part in p.parts), str(p)
        includes.append({'path': str(p), 'sha256': digest})
    assert includes
    bindings.append({'source': row['source'], 'includes': includes})
(ARTIFACT / 'RESOLVED_INCLUDES_FINAL.json').write_text(json.dumps(bindings, indent=2), encoding='utf-8')
result = {'scope': 'compiled COFF method bytes and relocations, not linked game image or runtime',
          'sentinels': {'82815570': hex(x360rd.u32(0x82815570)), '82F264A8': hex(x360rd.u32(0x82F264A8))},
          'exact_functions': comparisons, 'device_initialize': initialize,
          'resource_constructor': ctor, 'resource_constructor_crt_relocations': crt_refs,
          'source_storage_capacity_is_minimum_no_bind_contract': True,
          'original_full_cpp_size_alignment_unproved': True,
          'unique_resolved_include_paths': len({x['path'] for b in bindings for x in b['includes']}),
          'include_binding_count': sum(len(b['includes']) for b in bindings)}
(ARTIFACT / 'COFF_NATIVE_COMPARISON.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
print(json.dumps({'exact_functions': [{k: r[k] for k in ('symbol', 'length', 'exact_bytes_equal')} for r in comparisons],
                  'device_initialize_non_relocated_bits_equal': True,
                  'resource_ctor_five_zero_stores': True,
                  'unique_includes': result['unique_resolved_include_paths'],
                  'include_bindings': result['include_binding_count']}, indent=2))
