from pathlib import Path
import hashlib
import json
import random
import struct
import sys

parent = Path(r'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow')
sys.path.insert(0, str(parent / 'scratch/x360_sdk'))
sys.path.insert(0, str(parent / 'tools/re'))
import native_atomic_fixture as F
import x360rd

out = Path(r'D:/Reverse/BP-X360-Experimental-Artifacts/rendercore-suncorona-a449')
build = out / 'native-provider1'
paths = {Path(r['source']).name: Path(r['object']) for r in json.loads((build / 'compile-results.json').read_text())['results'] if r['passed']}
objects = [F.Coff(paths['vertexdescriptor.cpp'], '?Initialize@VertexDescriptor@'),
           F.Coff(paths['vertexformat.cpp'], '?VertexFormatGetStride@'),
           F.Coff(paths['device.cpp'], '?sub_82B61F90@')]
bundle = F.Bundle(objects)
SELF, AUX = F.M.S.SELF, F.M.S.AUX
DATA, PARAMS, RESOURCE = SELF + 0x1000, AUX + 0x1000, AUX + 0x2000
DEVICE, REPLACEMENT_DEVICE = AUX + 0x3000, AUX + 0x4000
ORIGINAL_DEVICE_GLOBAL = 0x83271608
create_export = json.loads(Path(r'D:/Reverse/BP-Decomp_Workflow/.ida-exports/BURNOUT_X360_ARTIST.XEX/0x82B611A0.json').read_text())
original_create = int(next(r['address'] for r in create_export['xrefs_from'] if r['name'] == 'D3DDevice_CreateVertexDeclaration'), 16)
rng = random.Random(0x82b63060)
errors = []
checks = {}
formats = [0x2a23b9, 0x2c23a5, 85126, 1712218, 1712291, 1712986, 2761657, 2891938, 0x7fffffff]

def pointer(c, reg):
    return c.g(reg) & 0xffffffff

def fresh():
    c = F.cpu(bundle)
    c.mem.add(ORIGINAL_DEVICE_GLOBAL, 4)
    return c

def map_hooks(c, callbacks):
    for address, name in bundle.unresolved.items():
        if name in callbacks:
            c.hooks[address] = callbacks[name]

for mode, prefix, original, cases in [
    ('initialize', '?Initialize@VertexDescriptor@', 0x82b63060, 512),
    ('create', '?CreateD3DObject@VertexDescriptor@', 0x82b611a0, 128)
]:
    bundle.select(prefix)
    if mode == 'initialize':
        table = F.address(bundle, '?gauVertexFormatDefaults@')
        assert bytes(bundle.images[table])[:32] == x360rd.rd(0x8214ad50, 32)
    for index in range(cases):
        data = bytearray(rng.randbytes(0x300))
        params = bytearray(rng.randbytes(0x110))
        count = index % 16 + 1
        active = set(rng.sample(range(16), count))
        for n in range(16):
            stream = 0 if index % 8 == 0 else (index + n) % 16
            offset = [0xffff, 0x100, 0, 4, 252][(index + n) % 5]
            fmt = 0xffffffff if n not in active else formats[(index + n) % len(formats)]
            method = (index + n) % 4
            usage = 0xff if n % 2 else (index + n) % 16
            usage_index = 0xff if n % 3 else (index + n) % 8
            element_type = (index + n * 3) % 16
            element_class = (index + n) % 3
            struct.pack_into('>HHI4BI', params, n * 16, stream, offset, fmt,
                             method, usage, usage_index, element_type, element_class)
            params[0x100 + n] = 0 if (index + n) % 3 else ((index * 7 + n) & 255)
        # Include the original do/while count-zero CreateD3DObject path.
        struct.pack_into('>H', data, 8, index % 17 if mode == 'create' else count)
        handles = struct.pack('>5I', DATA, 0x11223344, 0x55667788, index, 0xffffffff - index)
        both = []
        for compiled in (False, True):
            c = fresh()
            c.mem.wr(DATA, data)
            c.mem.wr(PARAMS, params)
            c.mem.wr(RESOURCE, handles)
            c.mem.wr(pointer(c, 1) - 0x2000, bytes([index & 255]) * 0x2000)
            c.setg(3, RESOURCE if mode == 'initialize' else DATA)
            c.setg(4, PARAMS)
            trace = []

            def memset(cpu):
                dest, value, size = pointer(cpu, 3), cpu.g(4) & 255, pointer(cpu, 5)
                trace.append(('memset', dest, value, size))
                cpu.mem.wr(dest, bytes([value]) * size)
                cpu.setg(3, dest)

            def create(cpu):
                n = max(int.from_bytes(cpu.mem.rd(DATA + 8, 2), 'big'), 1)
                trace.append(('create', pointer(cpu, 3) % 4,
                              cpu.mem.rd(pointer(cpu, 3), (n + 1) * 12).hex(), cpu.mem.rd(DATA, 0x300).hex()))
                cpu.setg(3, 0 if index % 7 == 0 else DEVICE + index * 4)

            c.hooks.update({0x82c0b180: memset, original_create: create})
            map_hooks(c, {'memset': memset, 'D3DDevice_CreateVertexDeclaration': create})
            c.run(bundle.entry if compiled else original)
            both.append((c.mem.rd(DATA, 0x300), c.mem.rd(PARAMS, 0x110), c.mem.rd(RESOURCE, 20),
                         pointer(c, 3), trace))
        if both[0] != both[1]:
            errors.append(dict(group=mode, case=index,
                differences=[hex(n) for n,(a,b) in enumerate(zip(both[0][0], both[1][0])) if a != b],
                traces=[result[-1] for result in both]))
    checks[mode] = cases

for mode, prefix, original in [
    ('unbind', '?sub_82B61F90@', 0x82b61f90),
    ('release', '?Release@VertexDescriptor@', 0x82b63250)
]:
    bundle.select(prefix)
    compiled_device_global = F.address(bundle, '?gpD3DDevice@')
    helper = F.address(bundle, '?sub_82B61F90@')
    for index in range(256):
        data = bytearray(rng.randbytes(0x80))
        initial_declaration = 0 if index % 4 == 0 else AUX + 0x8000 + index * 4
        struct.pack_into('>I', data, 0, initial_declaration)
        initial_device = 0 if index % 9 == 0 else DEVICE
        replacement_declaration = AUX + 0xa000 + index * 4
        is_set_result = [0, 1, 2, 0xffffffff][index % 4]
        both = []
        for compiled in (False, True):
            c = fresh()
            c.mem.wr(DATA, data)
            F.put(c, ORIGINAL_DEVICE_GLOBAL, initial_device)
            F.put(c, compiled_device_global, initial_device)
            c.setg(3, DATA)
            trace = []
            gp = compiled_device_global if compiled else ORIGINAL_DEVICE_GLOBAL

            def observe(pc, word):
                if pc == (helper if compiled else 0x82b61f90):
                    trace.append(('helper_this', pointer(c, 3)))

            def is_set(cpu):
                trace.append(('IsSet', pointer(cpu, 3), pointer(cpu, 4), cpu.mem.rd(DATA, 0x80).hex()))
                F.put(cpu, gp, REPLACEMENT_DEVICE)
                if index & 1:
                    F.put(cpu, DATA, replacement_declaration)
                cpu.setg(3, is_set_result)

            def set_declaration(cpu):
                trace.append(('SetVertexDeclaration', pointer(cpu, 3), pointer(cpu, 4)))
                assert pointer(cpu, 3) == initial_device
                assert pointer(cpu, 4) == 0

            def release(cpu):
                trace.append(('Release', pointer(cpu, 3), cpu.mem.rd(DATA, 0x80).hex()))
                F.put(cpu, DATA, 0xfedcba98)
                cpu.setg(3, 17)

            c.instruction_observer = observe
            c.hooks.update({0x82942118: is_set, 0x82943c80: set_declaration, 0x82941dc8: release})
            map_hooks(c, {'D3DResource_IsSet': is_set, 'D3DDevice_SetVertexDeclaration': set_declaration,
                          'D3DResource_Release': release})
            c.run(bundle.entry if compiled else original)
            both.append((c.mem.rd(DATA, 0x80), c.mem.rd(gp, 4), trace))
        if both[0] != both[1]:
            errors.append(dict(group=mode, case=index, original=both[0][2], compiled=both[1][2]))
    checks[mode] = 256

result = dict(checks=checks, errors=errors,
    objects={name:dict(path=str(path),sha256=hashlib.sha256(path.read_bytes()).hexdigest()) for name,path in paths.items()},
    scope='Actual complete native VertexDescriptor Initialize/CreateD3DObject/void Release plus original-identity sub_82B61F90 against ARTIST. Actual VertexFormatGetStride executes. SDK12-byte declaration including untouched padding/end sentinel, compacted element/state/stride bytes, five resource lanes, mutable declaration reload and captured-device ordering are compared.',
    limits='SDK CreateVertexDeclaration/IsSet/SetVertexDeclaration/Release and CRT memset are controlled. Element/stream types are bounded to0..15; direct Create count0 follows original do/while with mapped storage. No GPU, physical-device or full-link claim.')
report = out / 'native-provider1/vertex-provider-comparison.json'
assert not report.exists(), 'Refusing to overwrite completed evidence'
report.write_text(json.dumps(result, indent=2) + '\n')
print(json.dumps(dict(checks=checks, errors=len(errors), first_error=errors[:1]), indent=2))
raise SystemExit(bool(errors))
