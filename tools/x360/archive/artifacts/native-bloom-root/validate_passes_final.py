from pathlib import Path
import ast, hashlib, json, random, struct, sys

HERE = Path(__file__).resolve().parent
source = HERE / 'validate_constructor.py'
nodes = []
for node in ast.parse(source.read_text()).body:
    if isinstance(node, ast.Assign) and any(isinstance(t, ast.Name) and t.id == 'report_path' for t in node.targets):
        break
    nodes.append(node)
exec(compile(ast.Module(body=nodes, type_ignores=[]), str(source), 'exec'), globals())

class PassCPU(TintCPU):
    def step(self, pc, word, nxt, depth):
        if word >> 26 == 31 and ((word >> 1) & 1023) == 199:
            # Classic VMX stvewx: effective-address low nibble selects the
            # vector word; the store address is word aligned (big endian).
            vs, ra, rb = (word >> 21) & 31, (word >> 16) & 31, (word >> 11) & 31
            address = ((self.g(ra) if ra else 0) + self.g(rb)) & 0xFFFFFFFF
            self.mem.wr(address & ~3, struct.pack('>I', self.vr[vs][(address & 15) >> 2]))
            return nxt
        return super().step(pc, word, nxt, depth)
report_path = HERE / 'native-final/compile-results.json'
report = json.loads(report_path.read_text())
path = Path(report['results'][0]['object'])
specs = [(0x82401AE8, '?PrepareDownSampleBuffer@', 96),
         (0x82401F50, '?Generate1PassBlurredBloomBuffer@', 96),
         (0x82402308, '?Generate2PassBlurredBloomBuffer@', 96),
         (0x82402B40, '?Render@BrnPostFxBloom@', 128)]
BASE, SELF, RT0, RT1, RT2 = 0x58000000, 0x58000100, 0x58000400, 0x58000600, 0x58000800
CONSTANTS, VERTICES, DEVICE = BASE + 0x2000, BASE + 0x5000, BASE + 0x1000
rng = random.Random(0x82401AE8)
groups, errors = [], []
for original, prefix, count in specs:
    product = PackedCoff(path, prefix)
    bundle = PackedBundle([product]); bundle.select(prefix)
    for index in range(count):
        initial = bytearray(rng.randbytes(0x7000))
        def put(address, value):
            struct.pack_into('>I', initial, address - BASE, value & 0xFFFFFFFF)
        for offset in (0x18, 0x20, 0x24, 0x3C, 0x40, 0x50, 0x54):
            put(SELF + offset, BASE + 0x1100 + offset * 4)
        for n, offset in enumerate((0x28, 0x2C, 0x30, 0x34, 0x44, 0x48, 0x58, 0x5C, 0x60, 0x64, 0x68)):
            put(SELF + offset, 0x10001000 + n)
        width = [1, 3, 16, 47, 128, 640, 1024, 4096][index % 8]
        height = [2, 5, 32, 19, 720, 256, 13][index % 7]
        for n, rt in enumerate((RT0, RT1, RT2)):
            put(rt + 4, width); put(rt + 8, height); put(rt + 0x2C, BASE + 0x1400 + n * 0x40)
        threshold = f32(rng.uniform(-1, 0.99))
        white = f32(rng.uniform(0.01, 8))
        struct.pack_into('>2f', initial, SELF - BASE + 0x70, threshold, white)
        initial[SELF - BASE + 0x1C] = index % 2
        initial[SELF - BASE + 0x1D] = 0
        results = []
        for compiled in (False, True):
            c = cpu(bundle); c.__class__ = PassCPU; c.mem.add(BASE, len(initial), initial)
            c.mem.add(0x83010000, 0x2000)
            c.mem.add(0x83271000, 0x1000)
            trace, state_count, draw_count = [], [0], [0]
            # Original cache checks are held in their already-bound state. The
            # product external setters are controlled no-ops; no state proof.
            c.mem.wr(0x83010907, b'\x01')
            vertex_offset = 0x20 if original == 0x82401AE8 else (0x3C if original == 0x82401F50 else 0x50)
            F.put(c, 0x8301095C, struct.unpack_from('>I', initial, SELF - BASE + vertex_offset)[0])
            F.put(c, 0x83010958, struct.unpack_from('>I', initial, SELF - BASE + 0x18)[0])
            F.put(c, 0x83271608, DEVICE)
            def noop(state):
                state.setg(3, 1)
            def begin(state):
                trace.append(('Begin', state.g(3) & 0xFFFFFFFF, state.g(4) & 0xFFFFFFFF))
            def resolve(state):
                trace.append(('Resolve', *(state.g(r) & 0xFFFFFFFF for r in (3, 4, 5))))
            def pixel(state):
                trace.append(('Pixel', state.g(3) & 0xFFFFFFFF)); state.setg(3, 1)
            def states(state):
                handle = state.g(3) & 0xFFFFFFFF
                value = struct.unpack('>I', state.mem.rd(handle, 4))[0]
                trace.append(('Constant', handle, value))
                dest = CONSTANTS + state_count[0] * 0x20
                state_count[0] += 1
                state.mem.wr(state.g(4) & 0xFFFFFFFF, struct.pack('>2I', dest, value))
            def texture(state):
                trace.append(('Texture', state.g(3) & 0xFFFFFFFF, state.g(4) & 0xFFFFFFFF)); state.setg(3, 1)
            def flush(state):
                trace.append(('Flush',))
            def vertices(state):
                trace.append(('BeginVertices', *(state.g(r) & 0xFFFFFFFF for r in range(3, 7))))
                dest = VERTICES + draw_count[0] * 0x100
                draw_count[0] += 1
                state.setg(3, dest)
            def endvertices(state):
                trace.append(('EndVertices', state.g(3) & 0xFFFFFFFF))
            hooks = {0x823F9250: begin, 0x823F9338: resolve,
                     0x827E7D10: noop, 0x827E7CF8: noop, 0x82276C00: pixel,
                     0x822768D0: states, 0x8227D158: texture, 0x827E7A10: flush,
                     0x82944448: vertices, 0x82944978: endvertices,
                     0x82817548: noop, 0x82817558: noop,
                     0x82820810: lambda state: trace.append(('ASSERT', F.cstring(state, state.g(3) & 0xFFFFFFFF)))}
            c.hooks.update(hooks)
            for address, name in bundle.unresolved.items():
                if name.startswith('?Begin@RenderTarget@'): c.hooks[address] = begin
                elif name.startswith('?Resolve@RenderTarget@'): c.hooks[address] = resolve
                elif name.startswith('?BeginShaderStates@'): c.hooks[address] = states
                elif name.startswith('?SetPixelProgram@'): c.hooks[address] = pixel
                elif name.startswith('?SetState@Device@shadow@@SAXPBVTextureState@'): c.hooks[address] = texture
                elif name.startswith('?FlushVertexProgramState@'): c.hooks[address] = flush
                elif name == 'D3DDevice_BeginVertices': c.hooks[address] = vertices
                elif name == 'D3DDevice_EndVertices': c.hooks[address] = endvertices
                elif name.startswith('?gpD3DDevice@'): F.put(c, address, DEVICE)
                elif name.startswith('?sa'):
                    c.mem.wr(address, b'\x00' * 0x100)
                elif name.startswith(('?SetVertexProgram@', '?SetVertexDescriptor@', '?SetState@', '?BeginAssert@', '?EndAssert@')): c.hooks[address] = noop
                elif name.startswith('?FireAssert@'): c.hooks[address] = hooks[0x82820810]
                else: raise AssertionError(('uncontrolled', name))
            c.setg(3, SELF); c.setg(4, RT0); c.setg(5, RT1); c.setg(6, RT2)
            c.setf(1, threshold); c.setf(2, white)
            try:
                c.run(bundle.entry if compiled else original)
            except Exception:
                print('FAIL', hex(original), index, compiled, hex(getattr(c, 'last_pc', 0)), flush=True)
                raise
            results.append((c.mem.rd(BASE, len(initial)), trace, state_count[0], draw_count[0]))
        if results[0] != results[1]:
            errors.append({'function': hex(original), 'index': index,
                'different_bytes': [i for i, (a, b) in enumerate(zip(results[0][0], results[1][0])) if a != b],
                'original_vertices': results[0][0][VERTICES-BASE:VERTICES-BASE+0x200].hex(),
                'compiled_vertices': results[1][0][VERTICES-BASE:VERTICES-BASE+0x200].hex(),
                'original_trace': results[0][1], 'compiled_trace': results[1][1]})
    groups.append({'original': hex(original), 'symbol': product.fn[0], 'cases': count})
    print(prefix, count, 'checked', flush=True)
result = {'cases': sum(g['cases'] for g in groups), 'groups': groups,
          'mismatches': errors, 'object': str(path), 'object_sha256': sha(path),
          'report_sha256': sha(report_path), 'verifier_sha256': sha(__file__),
          'limits': 'Actual whole original/private passes and actual Render; original calls real private passes and product compiler inlines them. Full poisoned Bloom/render-target/constant/vertex/guard bytes plus ordered controlled boundary traces compared. Finite thresholds/white levels and nonzero rectangular dimensions; old blur dimensions equal, both Render modes. RenderTarget operations, pixel/texture state, constant destination allocation, flush, D3D vertex allocation/end controlled. Original shadow vertex/descriptor/blend caches prebound and product setters no-op: no device shadow state correctness claim. No GPU execution, hardware/status, failure assertions, full frame/game, link or global byte match proof.'}
output = HERE / (sys.argv[1] if len(sys.argv) > 1 else 'passes-final-comparison.json')
with output.open('x', encoding='utf-8') as f: json.dump(result, f, indent=2); f.write('\n')
print(json.dumps({'cases': result['cases'], 'mismatches': len(errors)}), flush=True)
assert not errors
