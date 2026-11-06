from pathlib import Path
import json, random, sys, struct, hashlib
P = Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow')
sys.path.insert(0, str(P / 'scratch/x360_sdk'))
import native_atomic_fixture as F
out = Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave8/xcam-native-terra/final')
row = next((x for x in json.loads(Path('D:\\Reverse\\BP-X360-Experimental-Artifacts\\wave8\\repair-r6\\current-object-witnesses\\xcam-decoder\\actual-row-selection.json').read_text())['results'] if x['source'].endswith('XCamDecoder.cpp')))
obj = F.Coff(Path(row['object']), '?DecodeFrame@CDecoder@XCAM@@')
bundle = F.Bundle([obj])
bundle.select('?DecodeFrame@CDecoder@XCAM@@')
SELF = F.M.S.SELF
AUX = F.M.S.AUX
CALLBACK = 2222993408
MASK = 4294967295

def branch(pc):
    w = F.M.V.x360rd.u32(pc)
    d = w & 67108860
    d = d - 67108864 if d & 33554432 else d
    return pc + d & MASK
orig = {'set': branch(2191032632), 'decode': branch(2191032648), 'close': branch(2191032760), 'init': branch(2191032768), 'reset': branch(2191032776), 'buffers': branch(2191032832), 'output': branch(2191032868), 'succeed': branch(2191032884)}

def fixture(compiled, status, skip, fails, initial, i):
    c = F.cpu(bundle)
    c.failures = fails
    c.msr = 32768
    trace = []
    rng = random.Random(821 + i)
    data = bytearray(rng.randbytes(512))
    struct.pack_into('>I', data, 0, initial)
    struct.pack_into('>I', data, 4, 1110726264)
    struct.pack_into('>I', data, 16, AUX + 256)
    c.mem.wr(SELF, data)
    F.put(c, AUX + 256, AUX + 512)
    F.put(c, AUX + 516, CALLBACK)

    def hook(name):

        def f(x):
            if name == 'set':
                trace.append([name, x.g(3) & MASK, x.g(4) & MASK])
            elif name == 'decode':
                trace.append([name, x.g(3) & MASK, x.g(5) & MASK])
                x.setg(3, status)
            elif name == 'callback':
                trace.append([name, x.g(3) & MASK])
            elif name in ('close', 'reset'):
                trace.append([name, x.g(3) & MASK])
                x.setg(3, 0)
            elif name == 'init':
                trace.append([name, x.g(3) & MASK])
                x.setg(3, 0)
            elif name == 'buffers':
                trace.append([name, x.g(3) & MASK])
                for k, reg in enumerate((4, 5, 6)):
                    F.put(x, x.g(reg) & MASK, 1140850688 + 256 * k)
                    F.put(x, (x.g(reg) & MASK) + 4, 64 + 16 * k)
            elif name == 'output':
                trace.append([name, x.g(3) & MASK, x.g(4) & MASK] + [x.g(j) & MASK for j in range(5, 11)])
                x.setg(3, 0)
            elif name == 'succeed':
                trace.append([name, x.g(3) & MASK])
        return f
    c.hooks[CALLBACK] = hook('callback')
    for name, addr in orig.items():
        c.hooks[addr] = hook(name)
    mapping = {'WMVideoDecSetErrorRecoveryFrameType': 'set', 'WMVideoDecDecodeData': 'decode', 'GetBuffersForWriting': 'buffers', 'WMVideoDecPlanarYUVGetOutput': 'output', 'BufferWriteSucceeded': 'succeed', 'WMVideoDecReset': 'reset', 'WMVideoDecClose': 'close'}
    for at, name in bundle.unresolved.items():
        for frag, k in mapping.items():
            if frag in name:
                c.hooks[at] = hook(k)
                break
    c.hooks[F.address(bundle, '?InitializeCodec@CDecoder@XCAM@@')] = hook('init')
    c.setg(3, SELF)
    c.setg(4, skip)
    c.setg(5, 17)
    c.run(bundle.entry if compiled else 2191032600)
    return {'return': c.g(3) & MASK, 'state': c.mem.rd(SELF, 512).hex(), 'trace': trace, 'atomic': c.atomic_trace, 'msr': c.msr}
errors = []
case = 0
for status in (-100, 0, 1, 3, 4, 11, 9):
    for skip in (0, 1):
        for fails in (0, 1, 3):
            for initial in (0, 4294967295, 2147483647):
                a = fixture(False, status, skip, fails, initial, case)
                b = fixture(True, status, skip, fails, initial, case)
                case += 1
                if a != b:
                    errors.append({'status': status, 'skip': skip, 'fails': fails, 'initial': initial, 'original': a, 'native': b})
result = {'scope': 'Whole actual DecodeFrame versus ARTIST under controlled codec, output and virtual notification services. State bytes, service call order/arguments, return, retry trace and interrupt state compared. Service implementations and concurrent hardware excluded.', 'cases': case, 'mismatches': errors, 'object': row['object'], 'object_sha256': hashlib.sha256(Path(row['object']).read_bytes()).hexdigest()}
_r5_output(out / 'decoder-comparison.json').write_text(json.dumps(result, indent=2) + '\n')
print('cases', case, 'errors', len(errors))
print(json.dumps(errors[:1], indent=2)[:7000])
assert not errors