from pathlib import Path
import json, random, sys, struct, hashlib
P = Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow')
sys.path.insert(0, str(P / 'scratch/x360_sdk'))
import native_atomic_fixture as F
out = Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave8/xaudio-xenon-native-terra/final')
row = json.loads(Path('D:\\Reverse\\BP-X360-Experimental-Artifacts\\wave8\\repair-r6\\current-object-witnesses\\xaudio-capture\\actual-row-selection.json').read_text())['results'][0]
obj = F.Coff(Path(row['object']), '?CaptureXBMAudioFrame@CXenonRenderer@XAUDIO@@')
bundle = F.Bundle([obj])
bundle.select('?CaptureXBMAudioFrame@CXenonRenderer@XAUDIO@@')
SELF = F.M.S.SELF
AUX = F.M.S.AUX
MASK = 4294967295
original_cursor = 2196993668
original_marker = 2200692228
native_cursor = F.address(bundle, 'guXBMCaptureCursor')
native_marker = F.address(bundle, 'guXBMResetMarker')

def branch(pc):
    w = F.M.V.x360rd.u32(pc)
    d = w & 67108860
    d = d - 67108864 if d & 33554432 else d
    return pc + d & MASK

def run(compiled, cursor, last, count, marker, fails, msr, seed):
    c = F.cpu(bundle)
    c.mem.add(2196992000, 4096)
    c.mem.add(2200690688, 4096)
    c.mem.add(1342177280, 65536)
    c.failures = fails
    c.msr = msr
    c.setg(13, 0)
    rng = random.Random(seed)
    data = bytearray(rng.randbytes(128))
    struct.pack_into('>II', data, 16, last, count)
    c.mem.wr(SELF, data)
    src = bytes(rng.randbytes(6144))
    c.mem.wr(AUX + 4096, src)
    F.put(c, AUX + 8, AUX + 4096)
    cp = native_cursor if compiled else original_cursor
    mp = native_marker if compiled else original_marker
    F.put(c, cp, cursor)
    F.put(c, mp, marker)
    trace = []

    def copy(x):
        d = x.g(3) & MASK
        s = x.g(4) & MASK
        n = x.g(5) & MASK
        trace.append(['memcpy', d, s, n, x.msr])
        x.mem.wr(d, x.mem.rd(s, n))
        x.setg(3, d)
    c.hooks[branch(2190880396)] = copy
    for at, name in bundle.unresolved.items():
        if name == 'memcpy':
            c.hooks[at] = copy
    c.setg(3, SELF)
    c.setg(4, AUX)
    c.run(bundle.entry if compiled else 2190880256)
    return {'return': c.g(3) & MASK, 'owner': c.mem.rd(SELF, 128).hex(), 'cursor': int.from_bytes(c.mem.rd(cp, 4), 'big'), 'marker': int.from_bytes(c.mem.rd(mp, 4), 'big'), 'ring': c.mem.rd(1342177280, 65536).hex(), 'atomic': [(w, val, ok) for addr, w, val, ok in c.atomic_trace], 'fences': [x[1] for x in c.fence_trace], 'msr': c.msr, 'calls': trace}
cases = []
for cursor in (0, 6144, 1342177280, 1342183424, 1342234624):
    for count in (0, 13, 14, 4294967295):
        for consecutive in (False, True):
            for fails in (0, 1, 3):
                for msr in (0, 32768):
                    cases.append((cursor, cursor - 6144 & MASK if consecutive else 305419896, count, 305441741, fails, msr))
errors = []
for i, args in enumerate(cases):
    try:
        a = run(False, *args, i)
        b = run(True, *args, i)
    except Exception as e:
        print('exception', i, args, repr(e))
        raise
    if a != b:
        errors.append({'case': i, 'input': args, 'fields': [k for k in a if a[k] != b[k]], 'original': {k: v for k, v in a.items() if k != 'ring'}, 'native': {k: v for k, v in b.items() if k != 'ring'}})
result = {'scope': 'Whole CaptureXBMAudioFrame actual ARTIST vs native COFF, controlled memcpy, reservation failures, owner and ring bytes, cursor/marker, MSR, return and call/fence order. Hardware concurrency and real memcpy excluded.', 'cases': len(cases), 'mismatches': errors, 'object': row['object'], 'object_sha256': hashlib.sha256(Path(row['object']).read_bytes()).hexdigest(), 'native_global_addresses': [hex(native_cursor), hex(native_marker)]}
_r5_output(out / 'capture-comparison.json').write_text(json.dumps(result, indent=2) + '\n')
print('cases', len(cases), 'mismatches', len(errors))
print(json.dumps(errors[:1], indent=2)[:4000])
assert not errors