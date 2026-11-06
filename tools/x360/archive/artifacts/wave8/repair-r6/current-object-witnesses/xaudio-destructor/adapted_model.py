from pathlib import Path
import json, random, sys, struct, hashlib
P = Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow')
sys.path.insert(0, str(P / 'scratch/x360_sdk'))
import native_atomic_fixture as F
out = Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave8/xaudio-xenon-native-terra/final')
row = json.loads(Path('D:\\Reverse\\BP-X360-Experimental-Artifacts\\wave8\\repair-r6\\current-object-witnesses\\xaudio-destructor\\actual-row-selection.json').read_text())['results'][0]
obj = F.Coff(Path(row['object']), '??1CXenonRenderer@XAUDIO@@')
b = F.Bundle([obj])
b.select('??1CXenonRenderer@XAUDIO@@')
SELF = F.M.S.SELF
MASK = 4294967295
nc = F.address(b, 'guXBMCaptureCursor')
nm = F.address(b, 'guXBMResetMarker')
oc = 2196993668
om = 2200692228
sym = {name: at for at, name in b.unresolved.items()}
prefix = lambda x: next((a for n, a in sym.items() if x in n))
actual = {prefix('?XenonRendererVTable@'): 2182123672, prefix('?XenonRendererRouterVTable@'): 2182123644, prefix('?ObjectRefCountVTable@'): 2182123508}

def branch(pc):
    w = F.M.V.x360rd.u32(pc)
    d = w & 67108860
    return pc + (d - 67108864 if d & 33554432 else d) & MASK

def run(comp, cursor, client, result, fails, msr, seed):
    c = F.cpu(b)
    c.mem.add(2196992000, 4096)
    c.mem.add(2200690688, 4096)
    c.failures = fails
    c.msr = msr
    c.setg(13, 0)
    data = bytearray(random.Random(seed).randbytes(64))
    struct.pack_into('>I', data, 24, client)
    c.mem.wr(SELF, data)
    cp = nc if comp else oc
    mp = nm if comp else om
    F.put(c, cp, cursor)
    F.put(c, mp, 2271560481)
    calls = []

    def unregister(x):
        calls.append([x.g(3) & MASK, int.from_bytes(x.mem.rd(cp, 4), 'big'), int.from_bytes(x.mem.rd(mp, 4), 'big'), x.msr])
        x.setg(3, result)
    c.hooks[branch(2190880580)] = unregister
    c.hooks[prefix('?XAudioUnregisterRenderDriverClient@')] = unregister
    c.setg(3, SELF)
    c.run(b.entry if comp else 2190880448)
    own = bytearray(c.mem.rd(SELF, 64))
    if comp:
        for off in (0, 4):
            x = int.from_bytes(own[off:off + 4], 'big')
            assert x in actual, (off, hex(x))
            struct.pack_into('>I', own, off, actual[x])
    return {'owner': own.hex(), 'cursor': int.from_bytes(c.mem.rd(cp, 4), 'big'), 'marker': int.from_bytes(c.mem.rd(mp, 4), 'big'), 'atomic': [(w, val, ok) for addr, w, val, ok in c.atomic_trace], 'msr': c.msr, 'calls': calls}
errors = []
i = 0
for cursor in (0, 1, 6144, 4294967295, 305419896):
    for client in (0, 1073816384):
        for result in (0, -1, 1):
            for fail in (0, 1, 3):
                for msr in (0, 32768):
                    args = (cursor, client, result, fail, msr, i)
                    a = run(False, *args)
                    c = run(True, *args)
                    i += 1
                    if a != c:
                        errors.append({'args': args, 'original': a, 'native': c})
result = {'scope': 'Actual ARTIST vs native COFF destructor; unregister service controlled, vtable symbol addresses normalized to ARTIST values. Owner bytes, globals, atomic attempts, EE state and unregister call ordering compared. Real vtable identities and service implementation outside scope.', 'cases': i, 'mismatches': errors, 'object': row['object'], 'object_sha256': hashlib.sha256(Path(row['object']).read_bytes()).hexdigest()}
_r5_output(out / 'destructor-comparison.json').write_text(json.dumps(result, indent=2) + '\n')
print('cases', i, 'mismatches', len(errors))
print(json.dumps(errors[:1], indent=2)[:4000])
assert not errors