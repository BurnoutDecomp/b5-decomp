from pathlib import Path
import hashlib
import json
import random
import struct
import sys

sys.path.insert(0, r'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/scratch/x360_sdk')
import native_atomic_fixture as F

out = Path(r'D:/Reverse/BP-X360-Experimental-Artifacts/rendercore-statefactories-eb06')
paths = {}
for folder in ['factory-native1', 'factory-consumers1', 'factory-final1']:
    rows = json.loads((out / folder / 'compile-results.json').read_text())['results']
    paths.update({Path(row['source']).name: Path(row['object']) for row in rows if row['passed']})

SELF, AUX = F.M.S.SELF, F.M.S.AUX
ALLOCATOR, VTABLE, BUFFER = SELF + 0x100, AUX + 0x100, AUX + 0x2000
ALLOCATE = 0x84803000
rng = random.Random(0x827ebba0)
errors = []
checks = []

for factory, provider, original, table, count, descriptor_ea, initialize_ea, parameter_size in [
    ('CgsRasterizerStateFactory', 'rasterizerstate', 0x827ebf30, 0x83010a38, 3, 0x82b63738, 0x82b62958, 0x20),
    ('CgsDepthStencilStateFactory', 'depthstencilstate', 0x827ebba0, 0x8301090c, 5, 0x82b636f8, 0x82b62890, 0x4c)
]:
    objects = [F.Coff(paths[factory + '.cpp'], '?Construct@' + factory + '@'),
               F.Coff(paths[provider + '.cpp'], '?Initialize@'),
               F.Coff(paths['BaseResourceDescriptor.cpp'], '??0BaseResourceDescriptor@')]
    bundle = F.Bundle(objects)
    bundle.select('?Construct@' + factory + '@')
    compiled_table = F.address(bundle, '?sa' + ('Rasterizer' if count == 3 else 'DepthStencil') + 'States@' + factory + '@')
    compiled_descriptor = F.address(bundle, '?GetResourceDescriptor@')
    compiled_initialize = F.address(bundle, '?Initialize@')
    for index in range(128):
        seed = rng.randbytes(count * 0x200)
        old_table = rng.randbytes(count * 4)
        allocator_seed = rng.randbytes(0x40)
        both = []
        for compiled in (False, True):
            c = F.cpu(bundle)
            c.mem.wr(ALLOCATOR, allocator_seed)
            F.put(c, ALLOCATOR, VTABLE)
            c.mem.wr(VTABLE, bytes(0x30))
            F.put(c, VTABLE + 0x10, ALLOCATE)
            c.mem.wr(BUFFER, seed)
            c.mem.add(table, count * 4, old_table)
            c.mem.wr(compiled_table, old_table)
            sp = c.g(1) & 0xffffffff
            c.mem.wr(sp - 0x2000, bytes([index & 255]) * 0x2000)
            c.setg(3, SELF)
            c.setg(4, ALLOCATOR)
            trace = []
            allocation_count = [0]
            current_table = compiled_table if compiled else table

            def table_bytes():
                return c.mem.rd(current_table, count * 4).hex()

            def observe(pc, word):
                if pc == (compiled_descriptor if compiled else descriptor_ea):
                    trace.append(('descriptor', c.mem.rd(c.g(4) & 0xffffffff, parameter_size).hex(), table_bytes()))
                if pc == (compiled_initialize if compiled else initialize_ea):
                    trace.append(('initialize', c.mem.rd(c.g(3) & 0xffffffff, 20).hex(),
                                  c.mem.rd(c.g(4) & 0xffffffff, parameter_size).hex(), table_bytes()))

            def allocate(cpu):
                slot = allocation_count[0]
                assert slot < count
                assert cpu.g(4) & 0xffffffff == ALLOCATOR
                assert cpu.g(6) & 0xffffffff == 0
                descriptor = cpu.mem.rd(cpu.g(5) & 0xffffffff, 40)
                trace.append(('allocate', descriptor.hex(), table_bytes(), cpu.mem.rd(BUFFER, count * 0x200).hex()))
                values = [BUFFER + slot * 0x200 + (index % 4) * 4,
                          AUX + 0x5000 + slot * 0x100, 0 if index % 2 else AUX + 0x6000 + slot * 0x100,
                          0xf0000000 | index, 0x12340000 | slot]
                cpu.mem.wr(cpu.g(3) & 0xffffffff, struct.pack('>5I', *values))
                allocation_count[0] += 1

            c.instruction_observer = observe
            c.hooks[ALLOCATE] = allocate
            c.run(bundle.entry if compiled else original)
            assert allocation_count[0] == count
            both.append((c.mem.rd(current_table, count * 4), c.mem.rd(BUFFER, count * 0x200),
                         c.mem.rd(ALLOCATOR, 0x40), trace))
        if both[0] != both[1]:
            errors.append({'factory': factory, 'case': index,
                           'state_differences': [n for n,(a,b) in enumerate(zip(both[0][1], both[1][1])) if a != b],
                           'traces': [value[-1] for value in both]})
    checks.append({'factory': factory, 'cases': 128, 'slots_per_case': count})

result = {'checks': checks, 'errors': errors,
          'objects': {name: {'path': str(path), 'sha256': hashlib.sha256(path.read_bytes()).hexdigest()} for name,path in paths.items()},
          'scope': 'Complete actual compiled factory Construct plus actual compiled state descriptor/Initialize and BaseResourceDescriptor constructor, against original827EBBA0/827EBF30 with all parameter bytes/padding, five descriptor/resource lanes, allocator slot16, table publication timing, and all state/output bytes compared.',
          'limits': 'Allocator DoAllocate is controlled. These are valid non-null allocation cases, so assertion and missing factory lifecycle link methods are outside scope. No full link, GPU or boot claim.'}
report = out / 'factory-final1/factory-comparison.json'
assert not report.exists(), 'Refusing to overwrite completed evidence'
report.write_text(json.dumps(result, indent=2) + '\n')
print(json.dumps({'checks':checks,'errors':len(errors),'first_error':errors[:1]}, indent=2))
raise SystemExit(bool(errors))
