from pathlib import Path
import ast, hashlib, json, random, struct

HERE = Path(__file__).resolve().parent
source = Path('D:/Reverse/BP-X360-Experimental-Artifacts/native-bloom-root/validate_constructor.py')
nodes = []
for node in ast.parse(source.read_text()).body:
    if isinstance(node, ast.Assign) and any(isinstance(t, ast.Name) and t.id == 'report_path' for t in node.targets):
        break
    nodes.append(node)
exec(compile(ast.Module(body=nodes, type_ignores=[]), str(source), 'exec'), globals())

class WindowDone(Exception):
    pass

class WindowCPU(TintCPU):
    def exec_vector(self, pc, word, mnemonic):
        if pc == self.window_stop:
            raise WindowDone()
        return super().exec_vector(pc, word, mnemonic)

report_path = HERE/'native-candidate-1/compile-results.json'
report = json.loads(report_path.read_text())
assert report['passed'] == 1 and report['failed'] == 0
obj_path = Path(report['results'][0]['object'])
BASE, SELF, START, PARAMS = 0x59000000, 0x59000100, 0x59000400, 0x59000A00
rng = random.Random(0x823322B8)
specs = [('count', '?Start@SurvivorMode@', 0x82332390, 0x82332404, 0x98, 0x110),
         ('ramp', '?Start@SurvivorMode@', 0x82332620, 0x8233267C, 0x320, 0x3B0),
         ('near_finish', '?PreWorldUpdate@SurvivorMode@', 0x8234D2C8, 0x8234D310, 0x118, 0x160)]
errors, groups, sensitivity = [], [], []
for name, prefix, original_start, original_stop, product_start, product_stop in specs:
    product = PackedCoff(obj_path, prefix)
    bundle = PackedBundle([product]); bundle.select(prefix)
    for index in range(256):
        initial = bytearray(rng.randbytes(0x2000))
        low = rng.randrange(0, 32)
        high = rng.randrange(low, 128)
        rank = f32(rng.uniform(-0.5, 1.5))
        if index % 4 == 0:
            threshold = rng.randrange(low, high+2)
            center = f32((threshold-low)/(high+1-low))
            bits = struct.unpack('>I', struct.pack('>f', center))[0]
            bits = max(0, bits + ((index//4) % 3 - 1))
            rank = struct.unpack('>f', struct.pack('>I', bits))[0]
        timer_low = f32(rng.uniform(-16, 32))
        timer_high = f32(rng.uniform(-16, 32))
        if index % 16 == 0: timer_low, timer_high, rank = 5.0, 5.0, 0.0
        elif index % 16 == 1: timer_low, timer_high, rank = -0.0, 0.0, 1.0
        distance = f32(rng.uniform(-50, 1999.999))
        previous = rng.randrange(-16, 128)
        wrecks = rng.randrange(32)
        for address in (START+0x338, PARAMS+4):
            struct.pack_into('>f', initial, address-BASE, rank)
        outputs = []
        for compiled in (False, True):
            c = cpu(bundle); c.__class__ = WindowCPU
            c.mem.add(BASE, len(initial), initial)
            c.mem.add(0x82CDB000, 0x1000, F.M.V.x360rd.rd(0x82CDB000, 0x1000))
            stack = c.g(1) & 0xFFFFFFFF
            c.window_stop = bundle.entry+product_stop if compiled else original_stop
            c.setg(29, START); c.setg(31, PARAMS)
            c.setg(3, wrecks); c.setg(23, 0); c.setg(24, 0)
            if name == 'count':
                c.setg(28, SELF); c.setg(30, SELF)
                F.put(c, 0x82CDB7BC, low); F.put(c, 0x82CDB7C0, high)
            elif name == 'ramp':
                c.setg(28, SELF if compiled else PARAMS+4); c.setg(30, SELF)
                c.mem.wr(0x82CDB7C4, struct.pack('>2f', timer_low, timer_high))
            else:
                c.setg(11, high); c.setf(1, distance)
                F.put(c, stack+0x58, previous)
            for address, symbol in bundle.unresolved.items():
                if symbol.startswith('?giMarkedManMinOpponentCount@'): F.put(c, address, low)
                elif symbol.startswith('?giMarkedManMaxOpponentCount@'): F.put(c, address, high)
                elif symbol.startswith('?gfMarkedManMinRampTime@'): c.mem.wr(address, struct.pack('>f', timer_low))
                elif symbol.startswith('?gfMarkedManMaxRampTime@'): c.mem.wr(address, struct.pack('>f', timer_high))
                elif symbol.startswith('?GetProgressionRankAsRatio@'):
                    def get_rank(state):
                        assert state.g(3) & 0xFFFFFFFF == START
                        state.setf(1, rank)
                    c.hooks[address] = get_rank
            try:
                c.run(bundle.entry+product_start if compiled else original_start)
            except WindowDone:
                pass
            else:
                raise AssertionError(('window did not reach exact boundary', name, index, compiled))
            outputs.append((c.mem.rd(BASE, len(initial)), c.g(30) & 0xFFFFFFFF if name == 'near_finish' else None))
        if outputs[0] != outputs[1]:
            errors.append({'window': name, 'index': index,
                'input': {'low':low, 'high':high, 'rank':rank, 'timer_low':timer_low, 'timer_high':timer_high, 'distance':distance, 'previous':previous},
                'different_bytes': [i for i,(a,b) in enumerate(zip(outputs[0][0], outputs[1][0])) if a != b],
                'original_register_result':outputs[0][1], 'compiled_register_result':outputs[1][1]})
        if name == 'count':
            actual = struct.unpack_from('>i', outputs[0][0], SELF-BASE+0xCC)[0]
            split = min(int(f32(f32(f32(float(high+1-low))*rank)+float(low))), high)
            if actual != split:
                sensitivity.append({'index': index, 'low':low, 'high':high, 'rank':rank, 'actual_fused_count':actual, 'separately_rounded_count':split})
    groups.append({'window':name, 'cases':256, 'original_start':hex(original_start), 'original_stop':hex(original_stop),
        'product_start':hex(bundle.entry+product_start), 'product_stop':hex(bundle.entry+product_stop), 'symbol':product.fn[0]})
    print(name, 256, 'checked', flush=True)
result = {'cases':768, 'mismatches':errors, 'groups':groups,
    'object':str(obj_path), 'object_sha256':sha(obj_path), 'report_sha256':sha(report_path), 'verifier_sha256':sha(__file__),
    'separate_rounding_counterexamples':sensitivity,
    'limits':'Actual changed arithmetic windows inside two whole-product XDK functions, no altered instruction/data bytes. Live registers are explicitly seeded at the documented window entries. Count uses actual original rank load; its unlinked product getter is controlled with exact this pointer and same value. All native global/count/ramp memory reads, fused math, truncation, cap branch, state/parameter stores and near-finish integer sum execute. Full poisoned object/input/output guards compared; stack scratch excluded because register allocation differs. Finite normal/zero inputs and representable integer conversions; no full Start/PreWorldUpdate or complete parameter/provider ABI, exceptional FP/status, gameplay/link/launch or byte-match claim.'}
p = HERE/'arithmetic-comparison.json'; assert not p.exists();p.write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
print(json.dumps({'cases':768,'mismatches':len(errors),'split_rounding_counterexamples':len(sensitivity)}),flush=True)
assert not errors
