from pathlib import Path
import ast, hashlib, json, re, struct

HERE = Path(__file__).resolve().parent
script = HERE / 'validate_native.py'
nodes = []
for node in ast.parse(script.read_text()).body:
    if isinstance(node, ast.Assert):
        break
    nodes.append(node)
exec(compile(ast.Module(body=nodes, type_ignores=[]), str(script), 'exec'), globals())

old_report = json.loads((HERE / 'native/compile-results.json').read_text())
new_report = json.loads((HERE / 'native-final/compile-results.json').read_text())
old_path = Path(old_report['results'][0]['object'])
new_path = Path(new_report['results'][0]['object'])
old = F.Coff(old_path, '?TintBlend@')
new = F.Coff(new_path, '?TintBlend@')
assert old.images == new.images and old.unresolved == new.unresolved and old.entry == new.entry

# The dispatcher reaches its table, six variants and all five weighted leaves.
retained = {'old_object': str(old_path), 'old_sha256': sha(old_path),
            'final_object': str(new_path), 'final_sha256': sha(new_path),
            'identical_relocated_sections': len(old.images),
            'identical_relocated_bytes': sum(map(len, old.images.values())),
            'unresolved_symbols_unchanged': old.unresolved,
            'source_delta': json.loads((HERE / 'formatting-only.json').read_text()),
            'basis': 'Every section reachable from the actual dispatcher/table, including six variants, five leaves and masks, is byte-identical after actual COFF relocation. Only a trailing blank source line changed; prior comparisons retain their original object/source bindings.'}
(HERE / 'formatting-retention.json').write_text(json.dumps(retained, indent=2))

section = new.sections[new.fn[2]]
body = new.data[section[4]:section[4] + section[3]]
raw_original = bytes(F.M.V.x360rd.rd(0x82AD4860, len(body)))
normalized = bytearray(raw_original)
relocations = []
for offset, index, kind in new.relocations(new.fn[2]):
    if kind == 0x12:
        assert index == 0
        continue
    assert kind in (0x10, 0x11), (offset, kind)
    word = struct.unpack_from('>I', normalized, offset)[0]
    struct.pack_into('>I', normalized, offset, word & 0xFFFF0000)
    relocations.append({'offset': offset, 'type': kind, 'symbol': new.symbols[index][0]})
assert len(body) == 28 and bytes(normalized) == body
(HERE / 'dispatcher-instruction-match.json').write_text(json.dumps({
    'original': '0x82AD4860', 'compiled_symbol': new.fn[0], 'length': len(body),
    'original_sha256': hashlib.sha256(raw_original).hexdigest(),
    'unrelocated_compiled_text_sha256': hashlib.sha256(body).hexdigest(),
    'relocations': relocations, 'identical_after_two_real_table_address_relocations': True,
    'limits': 'Seven instructions/28 bytes match after normalizing two actual data-address relocation immediates. Addresses are not yet placed at original image locations. This is one routine, not an image byte-match percentage.'}, indent=2))

mask_source = Path('D:/Reverse/BP-X360-Experimental-Worktrees/drive-thru-native-root/src/GameShared/Jobs/TintBlend/TintBlend.cpp')
text = mask_source.read_text()
mask_text = text.split('saTintMasks[13][16] =', 1)[1].split('};', 1)[0]
masks = bytes(int(x, 16) for x in re.findall(r'0x([0-9A-F]{2})\b', mask_text))
assert len(masks) == 208 and masks == bytes(F.M.V.x360rd.rd(0x821431A0, 208))

leaf = F.Coff(new_path, '?VMXBlend@')
outputs = []
for compiled in (False, True):
    c = cpu(leaf)
    c.mem.add(0x45000000, 0x1000, b'\xA5' * 0x1000)
    c.mem.wr(0x45000400, b'\xFF' * 48)
    c.mem.wr(0x45000500, b'\xFF' * 48)
    c.setg(3, 0x45000100)
    c.setg(4, 0x45000400)
    c.setg(5, 0x45000500)
    c.setvf(1, [1.0] * 4)
    c.setvf(2, [1.0] * 4)
    c.run(leaf.entry if compiled else 0x82AD2CF0)
    outputs.append(c.mem.rd(0x45000100, 64))
assert outputs[0] == outputs[1] == b'\xFF\xFE\xFE\xFE' * 16
old_scalar_clamped = b'\xFF' * 64
assert outputs[0] != old_scalar_clamped
(HERE / 'old-clamp-negative-witness.json').write_text(json.dumps({
    'actual_original_and_compiled_output': outputs[0].hex(),
    'old_source_byte_clamp_model': old_scalar_clamped.hex(),
    'different_bytes': sum(a != b for a,b in zip(outputs[0], old_scalar_clamped)),
    'cases': 1, 'old_actual_object': False,
    'limits': 'Actual original and final object run; old side is explicitly a model of the pre-existing scalar clamp, since the old source fails XDK compilation. Not a preserved old native object test.'}, indent=2))

headers = {}
artifacts = {}
for result in new_report['results']:
    for key in ('object', 'assembly', 'log'):
        p = Path(result[key])
        if p.exists():
            artifacts[str(p)] = sha(p)
    for line in Path(result['log']).read_text(errors='replace').splitlines():
        if 'Note: including file:' in line:
            p = Path(line.split('Note: including file:', 1)[1].strip())
            assert p.is_file(), p
            headers[str(p)] = sha(p)
prior_tools = json.loads(Path('D:/Reverse/BP-X360-Experimental-Artifacts/native-job-entries-root/DELIVERY.json').read_text())['compiler_and_runtime']
tools = {p: sha(p) for p in prior_tools}
assert tools == prior_tools
(HERE / 'actual-build-bindings.json').write_text(json.dumps({
    'actual_headers': headers, 'header_count': len(headers),
    'compiler_and_runtime': tools, 'artifacts': artifacts,
    'mask_original_address': '0x821431A0', 'mask_bytes': len(masks),
    'mask_sha256': hashlib.sha256(masks).hexdigest(),
    'mask_matches_original_raw_bytes': True, 'verifier_sha256': sha(__file__)}, indent=2))
print('Identical final relocated COFF sections', retained['identical_relocated_sections'],
      'bytes', retained['identical_relocated_bytes'], 'headers', len(headers),
      'dispatcher instruction bytes', len(body))
