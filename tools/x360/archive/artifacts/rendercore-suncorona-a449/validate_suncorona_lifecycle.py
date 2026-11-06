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
paths = {}
for folder in [out / 'native1', out / 'native-provider1',
               Path(r'D:/Reverse/BP-X360-Experimental-Artifacts/rendercore-statefactories-eb06/factory-consumers1')]:
    paths.update({Path(r['source']).name: Path(r['object']) for r in json.loads((folder / 'compile-results.json').read_text())['results'] if r['passed']})
objects = [F.Coff(paths['BrnSunCorona.cpp'], '?Construct@BrnSunCorona@'),
           F.Coff(paths['programbuffer.cpp'], '?Initialize@ProgramBuffer@'),
           F.Coff(paths['vertexdescriptor.cpp'], '?Initialize@VertexDescriptor@'),
           F.Coff(paths['vertexformat.cpp'], '?VertexFormatGetStride@'),
           F.Coff(paths['vertexdescriptorparameters.cpp'], '??0Parameters@VertexDescriptor@'),
           F.Coff(paths['CgsRwVertexDescResourceType.cpp'], '?GetResourceDescriptor@VertexDescriptor@'),
           F.Coff(paths['BaseResourceDescriptor.cpp'], '??0BaseResourceDescriptor@'),
           F.Coff(paths['device.cpp'], '?sub_82B61F90@')]
bundle = F.Bundle(objects)
SELF, AUX = F.M.S.SELF, F.M.S.AUX
SUN, ALLOCATOR, VTABLE = SELF + 0x1000, SELF + 0x2000, AUX + 0x100
ALLOCATE, FREE = 0x84803000, 0x84803010
DATA, PHYSICAL, CACHE, PHYSICAL_SOURCE, CTAB = [AUX + n for n in [0x3000, 0x6000, 0x8000, 0xa000, 0xc000]]
BLOBS = [(0x8203e118, 204), (0x8203e208, 524), (0x8203e438, 240), (0x8203e528, 464)]
blob_data = [x360rd.rd(address, size) for address,size in BLOBS]
rng = random.Random(0x824009b0)
errors = []
checks = {}

def ptr(cpu, reg):
    return cpu.g(reg) & 0xffffffff

def shader_index(cpu, address):
    matches = []
    for i, data in enumerate(blob_data):
        try:
            if cpu.mem.rd(address, len(data)) == data:
                matches.append(i)
        except KeyError:
            # COFF puts independent arrays in separately mapped data sections.
            continue
    assert len(matches) == 1, ('Unknown embedded shader', hex(address), matches)
    return matches[0]

def copy(cpu):
    dest, source, count = ptr(cpu, 3), ptr(cpu, 4), ptr(cpu, 5)
    cpu.mem.wr(dest, cpu.mem.rd(source, count))
    cpu.setg(3, dest)

def memset(cpu):
    dest, value, count = ptr(cpu, 3), cpu.g(4) & 255, ptr(cpu, 5)
    cpu.mem.wr(dest, bytes([value]) * count)
    cpu.setg(3, dest)

bundle.select('?Construct@BrnSunCorona@')
compiled_entries = {name:F.address(bundle, name) for name in [
    '?GetResourceDescriptor@VertexDescriptor@', '?Initialize@VertexDescriptor@',
    '?GetResourceDescriptor@ProgramBuffer@', '?Initialize@ProgramBuffer@',
    '?GetVariableHandleByName@ProgramBuffer@']}
for index in range(128):
    initial = rng.randbytes(0x80)
    state_seed = rng.randbytes(5 * 0x600)
    physical_seed = rng.randbytes(4 * 0x200)
    source_seed = rng.randbytes(4 * 0x400)
    allocator_seed = rng.randbytes(0x40)
    both = []
    for compiled in (False, True):
        c = F.cpu(bundle)
        c.mem.wr(SUN, initial)
        c.mem.wr(DATA, state_seed)
        c.mem.wr(PHYSICAL, physical_seed)
        c.mem.wr(CACHE, source_seed)
        c.mem.wr(PHYSICAL_SOURCE, source_seed)
        c.mem.wr(ALLOCATOR, allocator_seed)
        F.put(c, ALLOCATOR, VTABLE)
        c.mem.wr(VTABLE, bytes(0x30))
        F.put(c, VTABLE + 0x10, ALLOCATE)
        F.put(c, VTABLE + 0x14, FREE)
        c.mem.wr(ptr(c, 1) - 0x4000, bytes([index & 255]) * 0x4000)
        c.setg(3, SUN)
        c.setg(4, ALLOCATOR)
        trace = []
        allocation_count = [0]
        cache_lengths = [32 + ((index + n) % 3) * 16 for n in range(4)]
        physical_lengths = [16 + ((index + n) % 3) * 16 for n in range(4)]
        for shader in range(4):
            target = 'kUvStartAndOffset' if shader == 1 else 'kColourAndPower'
            names = ['unused' + str(shader), target] if shader % 2 else ['vertexUnused']
            if index % 5 == 0 and shader % 2:
                names[-1] = 'absentTarget'
            table = bytearray(0x200)
            struct.pack_into('>IIIII', table, 0, 28, 0, 0, len(names), 28)
            tail = 28 + 20 * len(names)
            for n,name in enumerate(names):
                data = name.encode() + b'\0'
                struct.pack_into('>IHHHHII', table, 28 + n * 20, tail, 2, (index + n * 3) % 64,
                                 1 + (index + n) % 4, 0, 0, 0)
                table[tail:tail + len(data)] = data
                tail += len(data)
            c.mem.wr(CTAB + shader * 0x400, table)

        def sun_bytes():
            return c.mem.rd(SUN, 0x54).hex()

        def observe(pc, word):
            if pc == (compiled_entries['?GetResourceDescriptor@VertexDescriptor@'] if compiled else 0x82b637d0):
                trace.append(('vertex_descriptor', c.mem.rd(ptr(c, 4), 0x110).hex(), sun_bytes()))
            if pc == (compiled_entries['?Initialize@VertexDescriptor@'] if compiled else 0x82b63060):
                trace.append(('vertex_initialize', ptr(c, 3) - SUN, c.mem.rd(ptr(c, 3), 20).hex(), sun_bytes()))
            if pc in [(compiled_entries['?GetResourceDescriptor@ProgramBuffer@'] if compiled else 0x82b634b0),
                      (compiled_entries['?Initialize@ProgramBuffer@'] if compiled else 0x82b62478)]:
                p = ptr(c, 4)
                words = struct.unpack('>10I', c.mem.rd(p, 40))
                values = [shader_index(c, words[0])] + [words[n] for n in [1, 2, 4, 6, 8, 9]]
                kind = 'program_descriptor' if pc == (compiled_entries['?GetResourceDescriptor@ProgramBuffer@'] if compiled else 0x82b634b0) else 'program_initialize'
                extra = c.mem.rd(ptr(c, 3), 20).hex() if kind == 'program_initialize' else ''
                trace.append((kind, values, extra, sun_bytes()))
            if pc == (compiled_entries['?GetVariableHandleByName@ProgramBuffer@'] if compiled else 0x82b609b0):
                trace.append(('lookup', ptr(c, 3), F.cstring(c, ptr(c, 4)), ptr(c, 5) - SUN,
                              c.mem.rd(ptr(c, 5), 4).hex(), sun_bytes()))

        def allocate(cpu):
            n = allocation_count[0]
            assert n < 5 and ptr(cpu, 4) == ALLOCATOR and ptr(cpu, 6) == 0
            trace.append(('allocate', n, cpu.mem.rd(ptr(cpu, 5), 40).hex(),
                          SUN <= ptr(cpu, 3) < SUN + 0x54, sun_bytes()))
            values = [DATA + n * 0x600, 0x11110000 | n,
                      PHYSICAL + max(n - 1, 0) * 0x200, 0x33330000 | index, 0x44440000 | n]
            cpu.mem.wr(ptr(cpu, 3), struct.pack('>5I', *values))
            allocation_count[0] += 1

        def parts(cpu):
            n = shader_index(cpu, ptr(cpu, 3))
            trace.append(('parts', n))
            cpu.mem.wr(ptr(cpu, 4), struct.pack('>4I', CACHE + n * 0x400, cache_lengths[n],
                                               PHYSICAL_SOURCE + n * 0x400, physical_lengths[n]))

        def constant_table(cpu):
            n = shader_index(cpu, ptr(cpu, 3))
            trace.append(('constant_table', n))
            F.put(cpu, ptr(cpu, 4), CTAB + n * 0x400)
            F.put(cpu, ptr(cpu, 5), 0x200)
            cpu.setg(3, 0)

        def shader_header(cpu, kind):
            words = struct.unpack('>4I', cpu.mem.rd(ptr(cpu, 5), 16))
            trace.append((kind, ptr(cpu, 3), ptr(cpu, 4), words))
            cpu.mem.wr(ptr(cpu, 3), cpu.mem.rd(words[0], ptr(cpu, 4)))

        def register(cpu, kind):
            trace.append((kind, ptr(cpu, 3), ptr(cpu, 4)))

        def create_declaration(cpu):
            records = [cpu.mem.rd(ptr(cpu, 3) + n * 12, 11).hex() for n in range(3)]
            trace.append(('create_declaration', records))
            cpu.setg(3, AUX + 0x18000 + index * 4)

        c.instruction_observer = observe
        c.hooks.update({ALLOCATE: allocate, 0x82c0a630: copy, 0x82926ba0: copy,
                        0x82c0b180: memset, 0x829435d0: parts,
                        0x82944128: lambda cpu: shader_header(cpu, 'vertex_header'),
                        0x82943fd0: lambda cpu: shader_header(cpu, 'pixel_header'),
                        0x82943968: lambda cpu: register(cpu, 'register_vertex'),
                        0x829436f8: lambda cpu: register(cpu, 'register_pixel')})
        const_export = json.loads(Path(r'D:/Reverse/BP-Decomp_Workflow/.ida-exports/BURNOUT_X360_ARTIST.XEX/0x82B608A8.json').read_text())
        const_ea = int(next(r['address'] for r in const_export['xrefs_from'] if r['name'] == 'XGMicrocodeGetConstantTable'), 16)
        c.hooks[const_ea] = constant_table
        create_export = json.loads(Path(r'D:/Reverse/BP-Decomp_Workflow/.ida-exports/BURNOUT_X360_ARTIST.XEX/0x82B611A0.json').read_text())
        create_ea = int(next(r['address'] for r in create_export['xrefs_from'] if r['name'] == 'D3DDevice_CreateVertexDeclaration'), 16)
        c.hooks[create_ea] = create_declaration
        hooks = {'memcpy': copy, 'XMemCpy': copy, 'memset': memset,
                 'XGGetMicrocodeShaderParts': parts, 'XGMicrocodeGetConstantTable': constant_table,
                 'XGSetVertexShaderHeader': lambda cpu: shader_header(cpu, 'vertex_header'),
                 'XGSetPixelShaderHeader': lambda cpu: shader_header(cpu, 'pixel_header'),
                 'XGRegisterVertexShader': lambda cpu: register(cpu, 'register_vertex'),
                 'XGRegisterPixelShader': lambda cpu: register(cpu, 'register_pixel'),
                 'D3DDevice_CreateVertexDeclaration': create_declaration}
        for address,name in bundle.unresolved.items():
            if name in hooks:
                c.hooks[address] = hooks[name]
        c.run(bundle.entry if compiled else 0x824009b0)
        assert allocation_count[0] == 5
        both.append((c.mem.rd(SUN, 0x80), c.mem.rd(DATA, len(state_seed)),
                     c.mem.rd(PHYSICAL, len(physical_seed)), trace))
    if both[0] != both[1]:
        errors.append(dict(group='construct',case=index,
            sun_differences=[hex(n) for n,(a,b) in enumerate(zip(both[0][0],both[1][0])) if a!=b],
            state_differences=[hex(n) for n,(a,b) in enumerate(zip(both[0][1],both[1][1])) if a!=b],
            first_trace_difference=next((n for n,(a,b) in enumerate(zip(both[0][3],both[1][3])) if a!=b),None),
            traces=[result[-1] for result in both]))
checks['construct_actual_providers'] = 128

result = dict(checks=checks,errors=errors,
    objects={name:dict(path=str(p),sha256=hashlib.sha256(p.read_bytes()).hexdigest()) for name,p in paths.items()},
    scope='Complete Sun Construct against ARTIST824009B0 with actual compiled VertexDescriptor parameter/sizer/Initialize/Create, stride provider, complete ProgramBuffer descriptor/Initialize/constant-table builder/name lookup, and BaseResourceDescriptor constructor. All five allocation/resource lanes, named handle IO, program order and complete Sun/allocated data bytes compare.',
    limits=['SDK shader-part/constant-table/header/register/declaration calls, CRT primitives and allocator DoAllocate are controlled. No GPU or boot claim.',
            'Compiled embedded binary pointers are normalized by exact blob bytes. Unwritten ProgramParameters pointer words and SDK declaration padding are not caller-stable stack bytes; defined fields/calls/payloads are compared.'])
report = out/'native-provider1/sun-lifecycle-comparison.json'
assert not report.exists(), 'Refusing to overwrite completed evidence'
report.write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps(dict(checks=checks,errors=len(errors),first_error=errors[:1]),indent=2))
raise SystemExit(bool(errors))
