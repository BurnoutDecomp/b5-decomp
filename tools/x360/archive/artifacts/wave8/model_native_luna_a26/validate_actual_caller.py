import ast, importlib.util, json, random, re, struct, sys, hashlib
from pathlib import Path
sys.path.insert(0, r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\scratch\x360_sdk')
import validate_native_math as M

# Calibrate the authoritative original reader before comparing ARTIST bytes.
spec = importlib.util.spec_from_file_location('authoritative_x360rd', r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\tools\re\x360rd.py')
XR = importlib.util.module_from_spec(spec); spec.loader.exec_module(XR)
assert XR.u32(0x82815570) == 0x7C8A2378 and XR.u32(0x82F264A8) == 4
assert M.V.x360rd.u32(0x82815570) == 0x7C8A2378 and M.V.x360rd.u32(0x82F264A8) == 4

# The original CRT initializer writes the native DWARF Vector4[5] table.
ORIG_TABLE = 0x830111C0
TABLE_BYTES = 80

def native_table_cpu():
    c = M.new_cpu()
    c.mem.add(ORIG_TABLE, 0x100)
    return c

orig_init = native_table_cpu()
orig_init.run(0x82C6BFE0)
original_table = orig_init.mem.rd(ORIG_TABLE, TABLE_BYTES)
assert original_table == b''.join(struct.pack('>4f', 0.0, 0.0, 0.0, float(i)) for i in range(5))

# Load the repository's COFF closure parser without importing its runner (which writes reports).
checker = Path(r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\scratch\x360_sdk\validate_render_states.py')
tree = ast.parse(checker.read_text(encoding='utf-8'))
ns = {'M': M, 'struct': struct, 're': re}
exec(compile(ast.Module(body=[n for n in tree.body if isinstance(n, ast.ClassDef) and n.name == 'CPU'], type_ignores=[]), str(checker), 'exec'), ns)
NativeCPU = ns['CPU']
sys.path.insert(0, r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\scratch\CRASHPARITY_0922\fxcrashvfx_vmxemu')
import emu64 as E
class StopBeforeShaderUpload(NativeCPU):
    def step(self, pc, word, nxt, depth):
        if pc == 0x827FBD1C:
            raise E.Return()
        return super().step(pc, word, nxt, depth)
NativeCPU = StopBeforeShaderUpload
exec(compile(ast.Module(body=[n for n in tree.body if isinstance(n, ast.ClassDef) and n.name == 'Coff'], type_ignores=[]), str(checker), 'exec'), ns)
Coff = ns['Coff']

obj = Path(r'D:\Reverse\BP-X360-Experimental-Artifacts\wave8\model_native_luna_a26\provider-compile-final\objects\CgsModel-f607d58d.obj')
setup = Coff(obj, '?SetupShaderConstantsForInstancing@Model@')
init = Coff(obj, '??__EgaTheFirstFewIntegersAsVectors@CgsGraphics@@YAXXZ')

def section_bases(coff):
    used, pending = set(), [coff.fn[2]]
    while pending:
        sec = pending.pop()
        if sec in used:
            continue
        used.add(sec)
        for at, index, kind in coff.relocations(sec):
            if kind == 0x12:
                continue
            _, _, target = coff.symbols[index]
            if target > 0:
                pending.append(target)
    return {sec: 0x83800000 + i * 0x10000 for i, sec in enumerate(sorted(used))}

def table_symbol(coff, bases):
    found = [v for v in coff.symbols.values() if v[0] == 'gaTheFirstFewIntegersAsVectors' and v[2] > 0]
    assert len(found) == 1, found
    name, value, sec = found[0]
    assert sec in bases, (name, sec, bases.keys())
    return bases[sec] + value

# Execute the compiled TU's own dynamic initializer; bind it byte-for-byte to ARTIST.
init_bases = section_bases(init)
init_table_addr = table_symbol(init, init_bases)
compiled_init = init.cpu()
compiled_init.__class__ = NativeCPU
compiled_init.run(init.entry)
compiled_table = compiled_init.mem.rd(init_table_addr, TABLE_BYTES)
assert compiled_table == original_table, (original_table.hex(), compiled_table.hex())

# The Model method's setup-local caller layout: four Vector4s immediately followed by
# four 64-byte Matrix44Affine values. The fifth 16-byte index read therefore aliases
# matrix[0]'s first row, exactly as the PPC caller at RenderRaceCar 0x822D1540..154C.
INDEX_BASE = 0x43000000
PTR_BASE = 0x41000000
STACK_BASE = 0x44000000
ORIGINAL_TABLE_BASE = 0x830111C0
CASE_COUNT = 64
rng = random.Random(0x822D154C)
report = {
    'target': 'actual compiled CgsModel SetupShaderConstantsForInstancing vs ARTIST PPC instructions',
    'compiled_symbol': setup.fn[0],
    'compiled_object': str(obj),
    'compiled_object_sha256': hashlib.sha256(obj.read_bytes()).hexdigest(),
    'compiled_setup_calls': setup.unresolved,
    'compiled_init_symbol': init.fn[0],
    'original_init_address': '0x82C6BFE0',
    'original_initializer_table_hex': original_table.hex(),
    'compiled_initializer_table_hex': compiled_table.hex(),
    'original_initializer_matches_compiled': original_table == compiled_table,
    'caller_layout': {
        'source': 'ARTIST BrnWorld::RaceCarEntityModule::RenderRaceCar @0x822CF6A0, call @0x822D154C',
        'index_input': 'exactly four Vector4 values (64 bytes)',
        'matrix_input': 'four adjacent Matrix44Affine values (4 * 64 bytes)',
        'provider_fifth_index_input': 'raw 16-byte first row of matrix[0] at index_base+0x40; no fifth Vector4 object is created',
        'matrix_pointer_table': 'separate four-entry pointer array passed in r4'
    },
    'cases': [], 'mismatches': [],
    'limits': 'bounded PPC interpreter and controlled SetShaderConstantArrayData hooks; no renderer, GPU, hardware, link, or game-runtime claim'
}

# For the compiled object, resolve the BSS address of the source table and seed it from
# the just-compared compiled initializer result. This avoids treating static CRT startup
# as part of the provider call while retaining the actual compiled table constructor.
setup_bases = section_bases(setup)
setup_table_addr = table_symbol(setup, setup_bases)

for case in range(CASE_COUNT):
    count = 1 + (case % 4)  # RaceCar wheel submission uses groups of one through four.
    vector_inputs = rng.randbytes(4 * 16)
    matrices = rng.randbytes(4 * 64)
    # This contiguous byte block models the two adjacent arrays in the original caller.
    index_and_matrix_block = vector_inputs + matrices
    pointer_table = b''.join(struct.pack('>I', INDEX_BASE + 64 + i * 64) for i in range(4))
    outputs = []

    # ARTIST entry and stack locals, stopping before shader table mutation at 0x827FBD1C.
    original = native_table_cpu()
    original.mem.wr(ORIGINAL_TABLE_BASE, original_table)
    original.mem.add(INDEX_BASE, len(index_and_matrix_block))
    original.mem.add(PTR_BASE, 0x100)
    original.mem.add(STACK_BASE, 0x20000)
    original.mem.wr(INDEX_BASE, index_and_matrix_block)
    original.mem.wr(PTR_BASE, pointer_table)
    original.setg(1, STACK_BASE + 0x10000)
    original.setg(3, count)
    original.setg(4, PTR_BASE)
    original.setg(5, INDEX_BASE)
    original.__class__ = NativeCPU
    original.run(0x827FBB98)
    original_sp = STACK_BASE + 0x10000 - 0x210
    original_arrays = (original.mem.rd(original_sp + 0x50, 5 * 16),
                       original.mem.rd(original_sp + 0xA0, 5 * 64))
    outputs.append(original_arrays)

    # Compiled XDK object. The native upload calls are intercepted at the exact call
    # boundary; all buffer bytes come from this original provider object.
    compiled = setup.cpu()
    compiled.__class__ = NativeCPU
    compiled.mem.wr(setup_table_addr, compiled_table)
    compiled.mem.add(INDEX_BASE, len(index_and_matrix_block))
    compiled.mem.add(PTR_BASE, 0x100)
    compiled.mem.wr(INDEX_BASE, index_and_matrix_block)
    compiled.mem.wr(PTR_BASE, pointer_table)
    captured = {}
    def capture(cpu):
        constant = cpu.g(4) & 0xFFFFFFFF
        ptr = cpu.g(5) & 0xFFFFFFFF
        length = 5 * 16 if constant == 7 else 5 * 64
        captured[constant] = cpu.mem.rd(ptr, length)
    for address, symbol in setup.unresolved.items():
        if symbol.startswith('?SetShaderConstantArrayData@ShaderConstantTable@@'):
            compiled.hooks[address] = capture
    compiled.setg(1, STACK_BASE + 0x10000)
    compiled.setg(3, count)
    compiled.setg(4, PTR_BASE)
    compiled.setg(5, INDEX_BASE)
    compiled.run(setup.entry)
    assert set(captured) == {6, 7}, (case, captured.keys(), setup.unresolved)
    outputs.append((captured[7], captured[6]))

    expected_index = bytearray()
    for i in range(4):
        row = bytearray(index_and_matrix_block[i * 16:(i + 1) * 16])
        row[12:16] = original_table[i * 16 + 12:i * 16 + 16]
        expected_index.extend(row)
    fifth = bytearray(index_and_matrix_block[64:80])
    fifth[12:16] = original_table[4 * 16 + 12:4 * 16 + 16]
    expected_index.extend(fifth)
    expected_matrices = matrices[:count * 64] + bytes((5 - count) * 64)
    expected = (bytes(expected_index), expected_matrices)
    assert outputs[0] == expected, ('ARTIST does not match caller-derived byte reference', case)
    assert outputs[1] == expected, ('compiled does not match caller-derived byte reference', case)
    report['cases'].append({
        'case': case, 'matrix_count': count,
        'index_and_matrix_input_sha256': hashlib.sha256(index_and_matrix_block).hexdigest(),
        'matrix_pointer_table_be': [hex(INDEX_BASE + 64 + i * 64) for i in range(4)],
        'original_index_output_sha256': hashlib.sha256(outputs[0][0]).hexdigest(),
        'compiled_index_output_sha256': hashlib.sha256(outputs[1][0]).hexdigest(),
        'original_matrix_output_sha256': hashlib.sha256(outputs[0][1]).hexdigest(),
        'compiled_matrix_output_sha256': hashlib.sha256(outputs[1][1]).hexdigest(),
        'index_w_bits': [hex(struct.unpack_from('>I', outputs[0][0], i * 16 + 12)[0]) for i in range(5)],
        'match': outputs[0] == outputs[1] == expected
    })

out = Path(r'D:\Reverse\BP-X360-Experimental-Artifacts\wave8\model_native_luna_a26\actual-caller-instancing-comparison.json')
out.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
print(json.dumps({'cases': len(report['cases']), 'mismatches': len(report['mismatches']),
                  'compiled_initializer_matches_original': report['original_initializer_matches_compiled'],
                  'index_w_bits_case0': report['cases'][0]['index_w_bits'], 'report': str(out)}, indent=2))
