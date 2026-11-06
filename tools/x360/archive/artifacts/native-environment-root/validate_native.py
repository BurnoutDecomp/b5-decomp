from pathlib import Path
import ast, hashlib, json, random, struct

HERE = Path(__file__).resolve().parent
parent_script = Path('D:/Reverse/BP-X360-Experimental-Artifacts/native-brnpostfx-root/validate_caller.py')
nodes = []
for node in ast.parse(parent_script.read_text()).body:
    if isinstance(node, ast.Assign) and any(isinstance(t, ast.Name) and t.id == 'report_path' for t in node.targets):
        break
    nodes.append(node)
exec(compile(ast.Module(body=nodes, type_ignores=[]), str(parent_script), 'exec'), globals())
assert F.M.V.x360rd.u32(0x82815570) == 0x7C8A2378
assert F.M.V.x360rd.u32(0x82F264A8) == 4
report_path = HERE / 'native-initial/compile-results.json'
report = json.loads(report_path.read_text())
path = Path(report['results'][0]['object'])
Return = F.CPU.run.__globals__['Return']

class WindowCPU(TintCPU):
    def exec_vector(self, pc, word, mnemonic):
        if pc == getattr(self, 'stop_pc', None):
            raise Return()
        try:
            return super().exec_vector(pc, word, mnemonic)
        except Exception:
            print('vector fault', hex(pc), hex(word), 'ra/rb', (word>>16)&31, (word>>11)&31, flush=True)
            raise
    def step(self, pc, word, nxt, depth):
        if pc == getattr(self, 'stop_pc', None):
            raise Return()
        return super().step(pc, word, nxt, depth)

BASE, SRC, DEST = 0x55000000, 0x55001000, 0x55002000
rng = random.Random(0x827D0098)
errors, functions = [], []
for name, prefix, start, stop in [
    ('copy', '??$CopyQuad@VVector3@', 0x827D03B0, 0x827D03BC),
    ('scale_rgb', '??$ScaleQuadRGB@VVector4@', 0x827D073C, 0x827D0748),
    ('scale_all', '??$ScaleQuad@VVector3@', 0x827D079C, 0x827D07A8)]:
    obj = PackedCoff(path, prefix)
    for index in range(256):
        image = rng.randbytes(0x3000)
        weight = f32([0.0, -0.0, 1.0, -1.0, 0.5, rng.uniform(-4.0, 4.0)][index % 6])
        results = []
        for compiled in (False, True):
            c = cpu(obj)
            c.__class__ = WindowCPU
            c.mem.add(BASE, len(image), image)
            if compiled:
                c.setg(3, DEST)
                c.setg(4, SRC)
                c.setf(1, weight)
            elif name == 'copy':
                c.setg(31, SRC - 0x5F0)
                c.setg(11, 0x5F0)
                c.setg(16, DEST)
            elif name == 'scale_rgb':
                c.setg(7, DEST)
                c.setvf(13, [weight, weight, weight, 1.0])
            else:
                c.setg(16, DEST)
                c.setvf(0, [weight] * 4)
            c.stop_pc = None if compiled else stop
            try:
                c.run(obj.entry if compiled else start)
            except Exception:
                print('failed', name, index, compiled, obj.fn, 'lastpc', hex(getattr(c, 'last_pc', 0)),
                      'gprs', [(r, hex(c.g(r))) for r in (3,4,7,11,16,31)], flush=True)
                raise
            results.append(c.mem.rd(BASE, len(image)))
        if results[0] != results[1]:
            errors.append({'kind': name, 'index': index, 'weight': weight,
                'original': results[0][DEST-BASE:DEST-BASE+16].hex(),
                'compiled': results[1][DEST-BASE:DEST-BASE+16].hex(),
                'different_bytes': [i for i,(a,b) in enumerate(zip(results[0], results[1])) if a != b]})
    functions.append({'kind': name, 'cases': 256, 'original_window': [hex(start),hex(stop)],
        'original_sha256': hashlib.sha256(F.M.V.x360rd.rd(start, stop-start)).hexdigest(),
        'compiled_actual_helper': obj.fn[0]})

ctor = PackedCoff(path, '??__EK_DEFAULT_JUNKYARD_KEY_LIGHT_DIRECTION')
bundle = PackedBundle([ctor])
bundle.select('??__EK_DEFAULT_JUNKYARD_KEY_LIGHT_DIRECTION')
target = []
for _, (name, value, section) in ctor.symbols.items():
    if name == 'K_DEFAULT_JUNKYARD_KEY_LIGHT_DIRECTION':
        target.append(bundle.bases[(0,section)] + value)
assert len(target) == 1, target
for index in range(64):
    poison = rng.randbytes(16)
    results = []
    for compiled in (False, True):
        c = cpu(bundle)
        address = target[0] if compiled else 0x8300F410
        if compiled:
            c.mem.wr(address, poison)
        else:
            c.mem.add(0x8300F400, 0x40, b'\xA5' * 0x40)
            c.mem.wr(address, poison)
        c.run(bundle.entry if compiled else 0x82C6AA08)
        results.append(c.mem.rd(address,16))
    if results[0] != results[1]:
        errors.append({'kind': 'global_initializer', 'index': index,
                       'original': results[0].hex(), 'compiled': results[1].hex()})
functions.append({'kind': 'global_initializer', 'cases': 64, 'original': '0x82C6AA08',
                  'compiled_symbol': bundle.symbol, 'actual_target': hex(target[0])})

result = {'cases': 832, 'mismatches': errors, 'functions': functions,
    'object': str(path), 'object_sha256': sha(path), 'native_report_sha256': sha(report_path),
    'verifier_sha256': sha(__file__),
    'limits': 'Actual emitted whole-product copy/scale helper code vs exact original three-instruction windows, matching logical pointer/weight inputs rather than whole GenerateShaderConstants ABI. Finite scale values, arbitrary raw128-bit source/destination lanes including NaNs, full data/guard region compared under shared softwareVMX/FP model. Scale weight register preseeded in original windows; its earlier permute construction is not tested. Actual complete compiled default initializer vs complete ARTIST82C6AA08, native variable locations normalized, randomized prior16B. Does not validate remaining scalar/trig/normalization algorithms, parser malformed inputs/uninitializedW, whole shader function, GPU/hardware, image/link/launch.'}
with (HERE / 'native-comparison.json').open('x', encoding='utf-8') as f:
    json.dump(result, f, indent=2)
print(json.dumps({'cases': result['cases'], 'mismatches': len(errors)}))
assert not errors
