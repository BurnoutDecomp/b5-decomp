from pathlib import Path
import ast, hashlib, importlib, inspect, json, math, random, struct, sys
source = Path('D:/Reverse/BP-X360-Experimental-Artifacts/replay_base_sol_r3/validate_construct.py')
nodes = []
for n in ast.parse(source.read_text()).body:
    if isinstance(n, ast.Assign) and any((isinstance(t, ast.Name) and t.id == 'stage' for t in n.targets)):
        break
    nodes.append(n)
exec(compile(ast.Module(body=nodes, type_ignores=[]), str(source), 'exec'), globals())
OUT = Path(__file__).resolve().parent
stage = sys.argv[1]
rows = json.loads(Path('D:\\Reverse\\BP-X360-Experimental-Artifacts\\wave8\\repair-r6\\current-object-witnesses\\physics-methods\\actual-row-selection.json').read_text())['results']
objects = [PackedCoff(Path(rows[0]['object']), '?CalculateNewVelocity@'), PackedCoff(Path(rows[1]['object']), '??0VecFloat@vpu@math@rw@@QAA@ABM@Z')]
all_methods = [('CalculateNewVelocity', 2186943248, 128), ('CalculateWorldIntertia', 2186944048, 64), ('CalculateCollisionImpulseWithInanimateObject', 2186922360, 64), ('CalculateCollisionImpulseWithBody', 2186922728, 64), ('DampenAngularVelocity', 2187013336, 96), ('DampPitchYawRoll', 2187059728, 96), ('GetImpulsesFromLocalImpulse', 2186943104, 64), ('IntegrateTransform', 2186967344, 32)]
methods = all_methods + [('AddLocalForce', 2186942064, 32), ('AddLocalImpulse', 2186942584, 32), ('AddLocalSpaceForce', 2187061224, 32)]
prefixes = ['?' + n + '@ExternalPhysicsBody@' for n, _, _ in methods] + ['??__EkvfSixty@']
loader = inspect.getsource(F.Bundle).replace('used, pending = set(), [(oi,sec)]', 'used, pending = set(), [(v[0],v[2])for n,v in self.definitions.items()if n.startswith(ROOT_PREFIXES)]')
loader = loader.replace('bases = {key:0x83800000+i*0x10000 for i,key in enumerate(sorted(used))}', 'bases = {}\n        cursor = 0x83800000\n        for key in sorted(used):\n            bases[key] = cursor\n            cursor += max(16,(self.objects[key[0]].sections[key[1]][3]+15)&~15)')
ns = dict(vars(importlib.import_module(F.Bundle.__module__)))
ns['ROOT_PREFIXES'] = tuple(prefixes)
exec(compile(loader, 'epb-stable-bundle', 'exec'), ns)
bundle = ns['Bundle'](objects)
entries = {}
for p in prefixes:
    bundle.select(p)
    entries[p] = bundle.entry
kv = [(value, sec) for name, value, sec in objects[0].symbols.values() if name == 'kvfSixty']
assert len(kv) == 1
sixty = bundle.bases[0, kv[0][1]] + kv[0][0]
primary = json.loads((OUT / 'INITIAL_BINDING.json').read_text())
runtime = {}
for d in primary['primary'].values():
    for x in d.get('xrefs_from', []):
        if x['name'].startswith(('__savevmx', '__restvmx')):
            runtime[x['name']] = int(x['address'], 16)
assert F.M.V.x360rd.u32(2193662408) == 327901647
assert F.M.V.x360rd.u32(2193663072) == 327901391
runtime.update({'__savevmx_124': 2193662404, '__restvmx_124': 2193663068})
bits = lambda f: struct.unpack('>I', struct.pack('>f', f))[0]
rng = random.Random(2186943248)
groups = []
init_bad = []

class MathCPU(TintCPU):

    def exec_vector(self, pc, word, label):
        if word >> 26 == 4 and word & 2047 == 644:
            dest = word >> 21 & 31
            a = word >> 16 & 31
            b = word >> 11 & 31
            self.vr[dest] = tuple((self.vr[a][i] >> (self.vr[b][i] & 31) for i in range(4)))
            return True
        if word >> 26 == 4 and word & 2047 in [458, 394, 586, 714]:
            op = word & 2047
            dest = word >> 21 & 31
            src = word >> 11 & 31
            values = []
            for raw in self.vf(src):
                x = float(raw)
                if op == 458:
                    y = float('nan') if math.isnan(x) or x < 0 else -float('inf') if x == 0 else math.log2(x)
                elif op == 394:
                    y = float('nan') if math.isnan(x) else float('inf') if x > 128 else 0.0 if x < -150 else 2.0 ** x
                elif not math.isfinite(x) or x == 0:
                    y = x
                elif op == 714:
                    y = float(math.floor(x))
                else:
                    y = float(math.trunc(x))
                    if y == 0:
                        y = math.copysign(0.0, x)
                try:
                    b = struct.unpack('>I', struct.pack('>f', y))[0]
                except OverflowError:
                    b = 4286578688 if y < 0 else 2139095040
                values.append(b)
            self.vr[dest] = tuple(values)
            return True
        return super().exec_vector(pc, word, label)

def new_cpu(compiled):
    c = cpu(bundle)
    c.__class__ = MathCPU
    for at, name in bundle.unresolved.items():
        if name in runtime:
            c.hooks[at] = lambda s, pc=runtime[name]: s.run(pc)
    c.mem.add(2197527280, 16, b'\xa5' * 16)
    c.run(entries['??__EkvfSixty@'] if compiled else 2193993776)
    assert c.mem.rd(sixty if compiled else 2197527280, 16) == struct.pack('>4f', 60.0, 60.0, 60.0, 60.0)
    return c
for name, original, cases in methods:
    prefix = '?' + name + '@ExternalPhysicsBody@'
    bad = []
    errors = []
    store_differences = []
    for index in range(cases):
        base, self, other, result, force, invA, invB = (1459617792, 1459617856, 1476395072, 1493172256, 1493172320, 1493172384, 1493172448)
        a = bytearray(rng.randbytes(416))
        b = bytearray(rng.randbytes(416))
        outs = rng.randbytes(320)
        for image in [a, b]:
            for off in [0, 16, 32, 48, 64, 80, 112, 128, 144, 160, 176, 192, 224, 240, 256, 272]:
                values = [rng.uniform(-3, 3) for _ in range(4)]
                if off in [0, 16, 32]:
                    values[:3] = [float(i == off // 16) for i in range(3)]
                struct.pack_into('>4f', image, 64 + off, *values)
            struct.pack_into('>4f', image, 64 + 208, *[rng.uniform(0.5, 50) for _ in range(4)])
        vectors = [[rng.uniform(-2, 2) for _ in range(4)] for _ in range(5)]
        vectors[0] = [rng.uniform(-0.1, 0.1) for _ in range(4)]
        if name.startswith('Damp'):
            vectors = [[rng.uniform(0.1, 0.99) for _ in range(4)] for _ in range(5)]
            dt = [rng.uniform(0.0, 0.05) for _ in range(4)]
            if index % 8 == 0:
                dt = [0.0, 0.0, 0.0, 0.0]
            if index % 8 == 1:
                dt = [0.0, 0.0, 0.0, 0.02]
            if index % 8 == 2:
                vectors[0] = [0.0, -0.0, -0.5, -1.0]
            if index % 8 == 3:
                vectors[0] = [1.0, -0.5, 2.0, 0.5]
            if name == 'DampenAngularVelocity':
                vectors[1] = dt
            else:
                vectors[3] = dt
        if name.startswith('CalculateCollision'):
            vectors[0] = [0.5, -0.8, 0.2, 1.1]
            vectors[1] = [-0.3, 0.7, 0.4, -0.5]
            vectors[2] = [1.0, -0.5, 0.3, 0.7]
            vectors[3] = [0.5, 0.6, 0.7, 0.8]
            vectors[4] = [0.1, 0.2, 0.3, 0.4]
        results = []
        case_errors = []
        for compiled in [False, True]:
            c = new_cpu(compiled)
            c.mem.add(base, len(a), a)
            c.mem.add(other - 64, len(b), b)
            c.mem.add(result - 32, len(outs), outs)
            events = []
            writes = []
            real_write = c.mem.wr

            def observe_write(address, data):
                if self <= address < self + 288:
                    writes.append(('body', hex(address - self), len(data)))
                if result <= address < result + 256:
                    writes.append(('out', hex(address - result), len(data)))
                real_write(address, data)
            c.mem.wr = observe_write

            def assert_return(s):
                events.append(('assert',))
            for at, symbol in bundle.unresolved.items():
                if symbol.startswith(('?BeginAssert@', '?FireAssert@', '?EndAssert@')):
                    c.hooks[at] = assert_return
            for at in [2189522248, 2189559824, 2189522264]:
                c.hooks[at] = assert_return
            c.setg(3, self)
            for i, v in enumerate(vectors, 1):
                c.vr[i] = tuple(map(bits, v))
            if name == 'CalculateCollisionImpulseWithInanimateObject':
                c.setg(3, result)
                c.setg(4, self)
                c.setg(5, force)
                c.setg(6, invA)
            elif name == 'CalculateCollisionImpulseWithBody':
                c.setg(3, result)
                c.setg(4, self)
                c.setg(5, other)
                c.setg(6, force)
                c.setg(7, invA)
                c.setg(8, invB)
            elif name == 'GetImpulsesFromLocalImpulse':
                c.setg(4, index % 3)
                c.setg(5, index // 3 % 3)
                c.setg(6, force)
                c.setg(7, invA)
            elif name in ['AddLocalForce', 'AddLocalImpulse']:
                c.setg(4, index % 3)
                c.setg(5, index // 3 % 3)
            try:
                c.run(entries[prefix] if compiled else original)
                results.append((c.mem.rd(base, len(a)), c.mem.rd(other - 64, len(b)), c.mem.rd(result - 32, len(outs)), events, c.g(3) & 4294967295 if name.startswith('CalculateCollision') else None, writes))
            except Exception as e:
                case_errors.append({'compiled': compiled, 'error': str(e), 'pc': hex(getattr(c, 'last_pc', 0))})
        if case_errors:
            errors.append({'case': index, 'errors': case_errors})
            continue
        if results[0][-1] != results[1][-1]:
            store_differences.append({'case': index, 'original': results[0][-1], 'compiled': results[1][-1]})
        if results[0][:-1] != results[1][:-1]:
            bad.append({'case': index, 'body_offsets': [hex(i - 64) for i, (x, y) in enumerate(zip(results[0][0], results[1][0])) if x != y], 'out_offsets': [hex(i - 32) for i, (x, y) in enumerate(zip(results[0][2], results[1][2])) if x != y], 'original_return': results[0][4], 'compiled_return': results[1][4], 'original_assert_events': len(results[0][3]), 'compiled_assert_events': len(results[1][3]), 'inputA': a.hex(), 'inputB': b.hex(), 'vectors': [list(map(hex, map(bits, v))) for v in vectors]})
    groups.append({'method': name, 'original': hex(original), 'cases': cases, 'mismatches': bad, 'exceptions': errors, 'store_sequence_differences': store_differences, 'last_case_original_stores': results[0][-1] if not case_errors else None, 'last_case_compiled_stores': results[1][-1] if not case_errors else None})
    print(name, 'cases', cases, 'mismatches', len(bad), 'exceptions', len(errors), flush=True)
report = {'stage': stage, 'groups': groups, 'objects': {str(o.path): hashlib.sha256(o.path.read_bytes()).hexdigest() for o in objects}, 'verifier_sha256': hashlib.sha256(Path(__file__).read_bytes()).hexdigest(), 'runtime_helper_bindings': {k: hex(v) for k, v in runtime.items()}, 'private_fixture_extensions': 'vsrw per-lane logical shift; vlogefp/vexptefp estimates modeled as rounded host log2/exp2; vrfim/vrfiz modeled with explicit infinities/NaN/signed-zero handling. Shared emulator source is unchanged; these are software limitations, not product fallbacks.', 'scope': 'Actual reconstructed EPB CPP and real SDK VecFloat constructor/constant initializer. Whole method body/object/output/guard comparison with unchanged retail argument registers and hidden member result pointers. Real compiled OrthoNormalize3x3 and WorldInertia are linked when reached. Runtime VMX save/restore helpers are bound to their original ABI implementations; assert services are controlled to return and only event counts compared. Shared software VMX estimate/rounding model, no hardware/denormal/actual UI, allocator, full physics/gameplay/launch claim. This first diagnostic preserves all failures rather than normalizing them.'}
p = _r5_output(OUT / (stage + '-native-final-v5.json'))
assert not p.exists()
_r5_output(p).write_text(json.dumps(report, indent=2) + '\n')
print('TOTAL', sum((g['cases'] for g in groups)), 'mismatch', sum((len(g['mismatches']) for g in groups)), 'exceptions', sum((len(g['exceptions']) for g in groups)))
assert not any((g['mismatches'] or g['exceptions'] for g in groups))
assert not any((g['store_sequence_differences'] for g in groups if g['method'] in ['CalculateNewVelocity', 'CalculateCollisionImpulseWithBody', 'CalculateCollisionImpulseWithInanimateObject']))