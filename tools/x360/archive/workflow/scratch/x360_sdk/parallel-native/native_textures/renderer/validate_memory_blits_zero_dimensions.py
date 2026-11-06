from pathlib import Path
import hashlib
import json
import random
import struct
import sys

parent = Path(r'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow')
sys.path.insert(0, str(parent / 'scratch/x360_sdk'))
import native_atomic_fixture as F

out = Path(sys.argv[1]) if len(sys.argv) > 1 else parent / 'b5-decomp/build/x360/parallel/native_textures/memory-blits-final2'
paths = {Path(r['source']).name: Path(r['object'])
         for r in json.loads((out / 'compile-results.json').read_text())['results']}
bundle = F.Bundle([F.Coff(paths[n], prefix) for n, prefix in
                   [('BrnRendererMemory.cpp', '?BlitDepth@'),
                    ('texturestate.cpp', '?Initialize@TextureState@'),
                    ('samplerstate.cpp', '?Initialize@SamplerState@')]])
SELF, AUX = F.M.S.SELF, F.M.S.AUX
IM, SOURCE0, SOURCE1 = AUX + 0x1000, AUX + 0x2000, AUX + 0x2200
TARGET0, TARGET1, STATE = AUX + 0x3000, AUX + 0x3200, AUX + 0x4000
ACTIVE, VP, DEVICE = 0x83010f9c, 0x8301095c, 0x83271608
DEFAULT, OFFSET = 0x830112d0, 0x82f2423e
errors = []
rng = random.Random(0x82406a68)
checks = 0

def rebase():
    addresses = {'?gpD3DDevice@@': DEVICE, '?mgDefault@Im2dTransform@': DEFAULT,
                 '?gBlitHalfPixelOffset82F2423E@@': OFFSET}
    for (oi, sec), base in bundle.bases.items():
        obj = bundle.objects[oi]
        body = bundle.images[base]
        for at, index, kind in obj.relocations(sec):
            name = obj.symbols[index][0]
            values = [v for p, v in addresses.items() if name.startswith(p)]
            if not values:
                continue
            value = values[0]
            word = struct.unpack_from('>I', body, at)[0]
            if kind in [0x10, 0x11]:
                word = (word & 0xffff0000) | (((value + 0x8000) >> 16 if kind == 0x10 else value) & 0xffff)
            elif kind == 2:
                word = value
            else:
                raise AssertionError((name, kind))
            struct.pack_into('>I', body, at, word)

for composite in [False, True]:
    bundle.select('?BlitComposite@' if composite else '?BlitDepth@')
    rebase()
    texture_init = F.address(bundle, '?Initialize@TextureState@') if not composite else None
    for i in range(256):
        module = bytearray(rng.randbytes(0x70))
        for offset in [0x2c, 0x30, 0x38, 0x3c]:
            struct.pack_into('>I', module, offset, AUX + 0x6000 + offset * 8)
        struct.pack_into('>5I', module, 0x48, STATE, *[rng.getrandbits(32) for _ in range(4)])
        initial_state = rng.randbytes(0x80)
        transform = rng.randbytes(64)
        width = [0, 1, 3, 320, 1280, 65535, 0x7fffffff, 0xffffffff][i % 8]
        height = [0, 3, 180, 720, 1080, 0xffff, 0x80000000, 0xffffffff][(i // 8) % 8]
        both = []
        for compiled in [False, True]:
            c = F.cpu(bundle)
            c.mem.add(0x83010000, 0x2000)
            c.mem.add(0x83271000, 0x1000)
            c.mem.add(0x82f24000, 0x1000)
            c.mem.wr(SELF, module)
            c.mem.wr(STATE, initial_state)
            c.mem.wr(DEFAULT, transform)
            c.mem.wr(OFFSET, bytes([i % 2]))
            F.put(c, DEVICE, AUX + 0x7000)
            F.put(c, ACTIVE, 0)
            F.put(c, VP, struct.unpack_from('>I', module, 0x38 if composite else 0x2c)[0]
                  if i % 4 == 0 else 0)
            F.put(c, SOURCE0 + 0x108, TARGET0)
            F.put(c, SOURCE1 + 0x108, TARGET1)
            for at, w, h in [(TARGET0, width, height), (TARGET1, height, width)]:
                F.put(c, at + 4, w)
                F.put(c, at + 8, h)
                F.put(c, at + 0x2c, AUX + 0x5000 + (at - TARGET0))
                F.put(c, at + 0x88, AUX + 0x5800 + (at - TARGET0))
            c.setg(3, SELF)
            c.setg(4, IM)
            c.setg(5, SOURCE0)
            c.setg(6, SOURCE1)
            c.setg(7, i % 2)
            c.setg(8, (i // 2) % 2)
            sp = c.g(1) & 0xffffffff
            c.mem.wr(sp - 0x2000, bytes([i]) * 0x2000)
            trace = []
            shader_rows = []
            counters = {'get_handle': 0, 'shader': 0, 'gpr': 0}

            def context():
                return c.mem.rd(SELF, 0x70).hex()

            def fire(x):
                trace.append(('assert', F.cstring(x, x.g(3) & 0xffffffff), F.cstring(x, x.g(4) & 0xffffffff), x.g(5) & 0xffffffff))

            def check_active(line):
                if struct.unpack('>I', c.mem.rd(ACTIVE, 4))[0] != IM + 4:
                    trace.append(('assert', 'mgpActiveRenderer == this',
                                  '..\\..\\..\\GameShared\\GameClasses\\Graphics/ImmediateMode/CgsImRenderer.h', line))

            def begin(x):
                trace.append(('begin', x.g(3) & 0xffffffff, context()))
                F.put(x, ACTIVE, IM + 4)
                if i % 7 == 0:
                    x.mem.wr(DEFAULT, bytes([0x5a]) * 64)
                    F.put(x, SOURCE0 + 0x108, TARGET1)

            def set_transform(x):
                trace.append(('transform', x.g(3) & 0xffffffff, x.mem.rd(x.g(4) & 0xffffffff, 64).hex()))
                if i % 7 == 1:
                    F.put(x, TARGET0 + 0x88, AUX + 0x5900)
                    for off in [0x2c, 0x30, 0x38, 0x3c]:
                        F.put(x, SELF + off, AUX + 0x6100 + off * 8)

            def texture(x, stage=None):
                state = x.g(3) & 0xffffffff
                unit = x.g(4) & 0xffffffff if stage is None else stage
                trace.append(('texture', state, unit, context()))
                if i % 7 == 2:
                    F.put(x, TARGET1 + 0x2c, AUX + 0x5c00)

            def set_state(x):
                check_active(881 if composite else 859)
                state = x.g(4) & 0xffffffff
                unit = x.g(5) & 0xffffffff if composite else 0
                trace.append(('texture', state, unit, context()))
                if i % 7 == 2:
                    F.put(x, TARGET1 + 0x2c, AUX + 0x5c00)

            def vertex_internal(x):
                trace.append(('vertex', x.g(3) & 0xffffffff, context()))
                if i % 7 == 3:
                    F.put(x, SELF + (0x3c if composite else 0x30), AUX + 0x6b00)

            def vertex(x):
                wanted = x.g(3) & 0xffffffff
                old = struct.unpack('>I', x.mem.rd(VP, 4))[0]
                if old != wanted:
                    vertex_internal(x)
                    F.put(x, VP, wanted)
                x.setg(3, old != wanted)

            def pixel(x):
                trace.append(('pixel', x.g(3) & 0xffffffff, context()))
                if i % 7 == 6:
                    x.mem.wr(OFFSET, bytes([1 - i % 2]))
                    F.put(x, TARGET0 + 4, 640)
                    F.put(x, TARGET0 + 8, 360)

            def get_handle(x):
                n = counters['get_handle']
                counters['get_handle'] += 1
                name = F.cstring(x, x.g(4) & 0xffffffff)
                trace.append(('handle', x.g(3) & 0xffffffff, name, context()))
                x.mem.wr(x.g(5) & 0xffffffff, bytes([0, n, int(n == 2), 1]))
                if i % 7 == 4 and n == 0:
                    F.put(x, SELF + 0x38, AUX + 0x6c00)
                x.setg(3, 1)

            def shader(x):
                n = counters['shader']
                counters['shader'] += 1
                handle = x.mem.rd(x.g(3) & 0xffffffff, 4)
                dest = AUX + 0x9000 + n * 0x20
                shader_rows.append(dest)
                trace.append(('shader', handle.hex(), context()))
                x.mem.wr(x.g(4) & 0xffffffff, struct.pack('>I', dest) + handle)
                if i % 7 == 5 and n == 1:
                    F.put(x, TARGET0 + 4, 1920)
                    F.put(x, TARGET0 + 8, 1080)

            def draw(x):
                count = x.g(6) & 0xffffffff
                trace.append(('draw', x.g(3) & 0xffffffff, x.g(4) & 0xffffffff, count,
                              x.mem.rd(x.g(5) & 0xffffffff, count * 20).hex(), context()))

            def end(x):
                check_active(645)
                F.put(x, ACTIVE, 0)

            def gpr(x):
                args = tuple(x.g(n) & 0xffffffff for n in range(3, 7))
                trace.append(('gpr', args, context()))
                counters['gpr'] += 1

            def reset(x):
                trace.append(('reset', context()))

            def observe(pc, word):
                if not composite and pc == (texture_init if compiled else 0x82b62720):
                    trace.append(('initialize', c.mem.rd(c.g(3) & 0xffffffff, 20).hex(),
                                  c.mem.rd(c.g(4) & 0xffffffff, 0x4c).hex(), context()))

            original = {0x823abed8: begin, 0x823ac048: set_transform, 0x8227d158: texture,
                        0x827e7cf8: vertex_internal, 0x82276c00: pixel, 0x82b609b0: get_handle,
                        0x822768d0: shader, 0x823c7030: draw, 0x82943e38: gpr,
                        0x82276970: reset, 0x82817548: lambda x: None,
                        0x82820810: fire, 0x82817558: lambda x: None}
            c.hooks.update(original)
            if compiled:
                for at, name in bundle.unresolved.items():
                    if name.startswith('?BeginRendering@'): c.hooks[at] = begin
                    elif name.startswith('?SetTransform@'): c.hooks[at] = set_transform
                    elif name.startswith('?SetState@ImRendererBase@'): c.hooks[at] = set_state
                    elif name.startswith('?SetVertexProgram@'): c.hooks[at] = vertex
                    elif name.startswith('?SetPixelProgram@'): c.hooks[at] = pixel
                    elif name.startswith('?GetVariableHandleByName@'): c.hooks[at] = get_handle
                    elif name.startswith('?BeginShaderStates@'): c.hooks[at] = shader
                    elif name.startswith('?Render@'): c.hooks[at] = draw
                    elif name.startswith('?EndRendering@'): c.hooks[at] = end
                    elif name.startswith('?ResetShadowing@'): c.hooks[at] = reset
                    elif name == 'D3DDevice_SetShaderGPRAllocation': c.hooks[at] = gpr
            c.instruction_observer = observe
            c.run(bundle.entry if compiled else (0x82406a68 if composite else 0x82406ea8))
            both.append((c.mem.rd(SELF, 0x70), c.mem.rd(STATE, 0x80),
                         c.mem.rd(ACTIVE, 4), c.mem.rd(VP, 4),
                         [c.mem.rd(a, 16) for a in shader_rows], trace))
        checks += 1
        if both[0] != both[1]:
            errors.append(dict(method='Composite' if composite else 'Depth', case=i,
                               module_differences=[hex(n) for n, (a, b) in enumerate(zip(both[0][0], both[1][0])) if a != b],
                               state_differences=[hex(n) for n, (a, b) in enumerate(zip(both[0][1], both[1][1])) if a != b],
                               shader_rows=[[r.hex() for r in v[4]] for v in both],
                               traces=[v[5] for v in both]))

result = dict(cases=checks, errors=errors,
              objects={n: dict(path=str(p), sha256=hashlib.sha256(p.read_bytes()).hexdigest()) for n, p in paths.items()},
              scope='256 whole native depth and256 composite blit cases. Actual TextureState/SamplerState initialization executes. '
                    'All Memory/state bytes, parameters/padding, resource lanes, sampler/program/transform/draw service I/O, '
                    'shader rows, active renderer, vertex cache, late producer mutations and call order compare.',
              limits='Im2d, shadow-device, program lookup, shader publication and SDK GPR allocation are controlled services. '
                     'This does not establish their bodies or a real GPU/frame. Source dimensions include zero and the full unsigned range. '
                     'The half-texel global has the original identity/initial byte; its descriptive C++ symbol is not claimed as a recovered debug name.')
report = out / 'memory-blit-zero-dimension-comparison.json'
assert not report.exists(), 'Refusing to overwrite completed comparison evidence'
report.write_text(json.dumps(result, indent=2) + '\n')
print(checks, 'whole Memory blit cases; errors', len(errors))
print(errors[:1])
raise SystemExit(bool(errors))
