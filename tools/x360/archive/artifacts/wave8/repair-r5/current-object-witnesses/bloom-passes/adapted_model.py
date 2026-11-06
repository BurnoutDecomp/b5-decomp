from pathlib import Path
import ast, hashlib, json, random, struct, sys
HERE = Path(__file__).resolve().parent
source = HERE / 'validate_constructor.py'
nodes = []
for node in ast.parse(source.read_text()).body:
    if isinstance(node, ast.Assign) and any((isinstance(t, ast.Name) and t.id == 'report_path' for t in node.targets)):
        break
    nodes.append(node)
exec(compile(ast.Module(body=nodes, type_ignores=[]), str(source), 'exec'), globals())

class PassCPU(TintCPU):

    def step(self, pc, word, nxt, depth):
        if word >> 26 == 31 and word >> 1 & 1023 == 199:
            vs, ra, rb = (word >> 21 & 31, word >> 16 & 31, word >> 11 & 31)
            address = (self.g(ra) if ra else 0) + self.g(rb) & 4294967295
            self.mem.wr(address & ~3, struct.pack('>I', self.vr[vs][(address & 15) >> 2]))
            return nxt
        return super().step(pc, word, nxt, depth)
report_path = Path('D:\\Reverse\\BP-X360-Experimental-Artifacts\\wave8\\repair-r5\\current-object-witnesses\\bloom-passes\\actual-row-selection.json')
report = json.loads(report_path.read_text())
path = Path(report['results'][0]['object'])
specs = [(2185239272, '?PrepareDownSampleBuffer@', 96), (2185240400, '?Generate1PassBlurredBloomBuffer@', 96), (2185241352, '?Generate2PassBlurredBloomBuffer@', 96), (2185243456, '?Render@BrnPostFxBloom@', 128)]
BASE, SELF, RT0, RT1, RT2 = (1476395008, 1476395264, 1476396032, 1476396544, 1476397056)
CONSTANTS, VERTICES, DEVICE = (BASE + 8192, BASE + 20480, BASE + 4096)
rng = random.Random(2185239272)
groups, errors = ([], [])
for original, prefix, count in specs:
    product = PackedCoff(path, prefix)
    bundle = PackedBundle([product])
    bundle.select(prefix)
    for index in range(count):
        initial = bytearray(rng.randbytes(28672))

        def put(address, value):
            struct.pack_into('>I', initial, address - BASE, value & 4294967295)
        for offset in (24, 32, 36, 60, 64, 80, 84):
            put(SELF + offset, BASE + 4352 + offset * 4)
        for n, offset in enumerate((40, 44, 48, 52, 68, 72, 88, 92, 96, 100, 104)):
            put(SELF + offset, 268439552 + n)
        width = [1, 3, 16, 47, 128, 640, 1024, 4096][index % 8]
        height = [2, 5, 32, 19, 720, 256, 13][index % 7]
        for n, rt in enumerate((RT0, RT1, RT2)):
            put(rt + 4, width)
            put(rt + 8, height)
            put(rt + 44, BASE + 5120 + n * 64)
        threshold = f32(rng.uniform(-1, 0.99))
        white = f32(rng.uniform(0.01, 8))
        struct.pack_into('>2f', initial, SELF - BASE + 112, threshold, white)
        initial[SELF - BASE + 28] = index % 2
        initial[SELF - BASE + 29] = 0
        results = []
        for compiled in (False, True):
            c = cpu(bundle)
            c.__class__ = PassCPU
            c.mem.add(BASE, len(initial), initial)
            c.mem.add(2197880832, 8192)
            c.mem.add(2200375296, 4096)
            trace, state_count, draw_count = ([], [0], [0])
            c.mem.wr(2197883143, b'\x01')
            vertex_offset = 32 if original == 2185239272 else 60 if original == 2185240400 else 80
            F.put(c, 2197883228, struct.unpack_from('>I', initial, SELF - BASE + vertex_offset)[0])
            F.put(c, 2197883224, struct.unpack_from('>I', initial, SELF - BASE + 24)[0])
            F.put(c, 2200376840, DEVICE)

            def noop(state):
                state.setg(3, 1)

            def begin(state):
                trace.append(('Begin', state.g(3) & 4294967295, state.g(4) & 4294967295))

            def resolve(state):
                trace.append(('Resolve', *(state.g(r) & 4294967295 for r in (3, 4, 5))))

            def pixel(state):
                trace.append(('Pixel', state.g(3) & 4294967295))
                state.setg(3, 1)

            def states(state):
                handle = state.g(3) & 4294967295
                value = struct.unpack('>I', state.mem.rd(handle, 4))[0]
                trace.append(('Constant', handle, value))
                dest = CONSTANTS + state_count[0] * 32
                state_count[0] += 1
                state.mem.wr(state.g(4) & 4294967295, struct.pack('>2I', dest, value))

            def texture(state):
                trace.append(('Texture', state.g(3) & 4294967295, state.g(4) & 4294967295))
                state.setg(3, 1)

            def flush(state):
                trace.append(('Flush',))

            def vertices(state):
                trace.append(('BeginVertices', *(state.g(r) & 4294967295 for r in range(3, 7))))
                dest = VERTICES + draw_count[0] * 256
                draw_count[0] += 1
                state.setg(3, dest)

            def endvertices(state):
                trace.append(('EndVertices', state.g(3) & 4294967295))
            hooks = {2185204304: begin, 2185204536: resolve, 2189327632: noop, 2189327608: noop, 2183621632: pixel, 2183620816: states, 2183647576: texture, 2189326864: flush, 2190754888: vertices, 2190756216: endvertices, 2189522248: noop, 2189522264: noop, 2189559824: lambda state: trace.append(('ASSERT', F.cstring(state, state.g(3) & 4294967295)))}
            c.hooks.update(hooks)
            for address, name in bundle.unresolved.items():
                if name.startswith('?Begin@RenderTarget@'):
                    c.hooks[address] = begin
                elif name.startswith('?Resolve@RenderTarget@'):
                    c.hooks[address] = resolve
                elif name.startswith('?BeginShaderStates@'):
                    c.hooks[address] = states
                elif name.startswith('?SetPixelProgram@'):
                    c.hooks[address] = pixel
                elif name.startswith('?SetState@Device@shadow@@SAXPBVTextureState@'):
                    c.hooks[address] = texture
                elif name.startswith('?FlushVertexProgramState@'):
                    c.hooks[address] = flush
                elif name == 'D3DDevice_BeginVertices':
                    c.hooks[address] = vertices
                elif name == 'D3DDevice_EndVertices':
                    c.hooks[address] = endvertices
                elif name.startswith('?gpD3DDevice@'):
                    F.put(c, address, DEVICE)
                elif name.startswith('?sa'):
                    c.mem.wr(address, b'\x00' * 256)
                elif name.startswith(('?SetVertexProgram@', '?SetVertexDescriptor@', '?SetState@', '?BeginAssert@', '?EndAssert@')):
                    c.hooks[address] = noop
                elif name.startswith('?FireAssert@'):
                    c.hooks[address] = hooks[2189559824]
                else:
                    raise AssertionError(('uncontrolled', name))
            c.setg(3, SELF)
            c.setg(4, RT0)
            c.setg(5, RT1)
            c.setg(6, RT2)
            c.setf(1, threshold)
            c.setf(2, white)
            try:
                c.run(bundle.entry if compiled else original)
            except Exception:
                print('FAIL', hex(original), index, compiled, hex(getattr(c, 'last_pc', 0)), flush=True)
                raise
            results.append((c.mem.rd(BASE, len(initial)), trace, state_count[0], draw_count[0]))
        if results[0] != results[1]:
            errors.append({'function': hex(original), 'index': index, 'different_bytes': [i for i, (a, b) in enumerate(zip(results[0][0], results[1][0])) if a != b], 'original_vertices': results[0][0][VERTICES - BASE:VERTICES - BASE + 512].hex(), 'compiled_vertices': results[1][0][VERTICES - BASE:VERTICES - BASE + 512].hex(), 'original_trace': results[0][1], 'compiled_trace': results[1][1]})
    groups.append({'original': hex(original), 'symbol': product.fn[0], 'cases': count})
    print(prefix, count, 'checked', flush=True)
result = {'cases': sum((g['cases'] for g in groups)), 'groups': groups, 'mismatches': errors, 'object': str(path), 'object_sha256': sha(path), 'report_sha256': sha(report_path), 'verifier_sha256': sha(__file__), 'limits': 'Actual whole original/private passes and actual Render; original calls real private passes and product compiler inlines them. Full poisoned Bloom/render-target/constant/vertex/guard bytes plus ordered controlled boundary traces compared. Finite thresholds/white levels and nonzero rectangular dimensions; old blur dimensions equal, both Render modes. RenderTarget operations, pixel/texture state, constant destination allocation, flush, D3D vertex allocation/end controlled. Original shadow vertex/descriptor/blend caches prebound and product setters no-op: no device shadow state correctness claim. No GPU execution, hardware/status, failure assertions, full frame/game, link or global byte match proof.'}
output = HERE / (sys.argv[1] if len(sys.argv) > 1 else 'passes-final-comparison.json')
with output.open('x', encoding='utf-8') as f:
    json.dump(result, f, indent=2)
    f.write('\n')
print(json.dumps({'cases': result['cases'], 'mismatches': len(errors)}), flush=True)
assert not errors