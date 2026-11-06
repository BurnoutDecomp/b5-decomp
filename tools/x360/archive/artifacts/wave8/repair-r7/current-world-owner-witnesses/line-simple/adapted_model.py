import sys, random, json, struct, hashlib
from pathlib import Path
sys.path.insert(0, 'C:\\Users\\adri1\\.codex\\worktrees\\fbaf\\BP-Decomp_Workflow\\scratch\\x360_sdk')
import native_atomic_fixture as F

class RunCPU(F.AtomicCPU):

    def step(self, pc, w, nxt, depth):
        if pc in getattr(self, 'rest_stubs', set()):
            raise F.CPU.run.__globals__['Return']()
        if w >> 26 == 31 and w >> 1 & 1023 == 444 and w & 1:
            src = w >> 21 & 31
            dst = w >> 16 & 31
            rhs = w >> 11 & 31
            value = self.g(src) | self.g(rhs)
            self.setg(dst, value)
            self.setcr(0, bool(value >> 63), bool(value and (not value >> 63)), value == 0)
            return nxt
        return super().step(pc, w, nxt, depth)
out = Path('D:\\Reverse\\BP-X360-Experimental-Artifacts\\parallel\\wave8_scene_terra\\volume-baseline')
paths = [_r5_object(out / 'closure-selected-v1/LineSegIntersect.obj'), _r5_object(out / 'closure-selected-v1/GPTriangle.obj'), _r5_object(out / 'closure-selected-v1/AALineClipper.obj')]
b = F.Bundle([F.Coff(paths[0], '?LineSegIntersect@BoxVolume@'), F.Coff(paths[1], '?TriangleNearestPointRegion@'), F.Coff(paths[2], '?Init@AALineClipper@')])
rng = random.Random(2193265784)
bad = []
fail = []
counts = {}
for name, orig in [('?LineSegIntersect@BoxVolume@', 2193265784), ('?LineSegIntersect@CapsuleVolume@', 2193292536), ('?ThinLineSegIntersect@CylinderVolume@', 2193284320)]:
    b.select(name)
    counts[name] = {'hit': 0, 'miss': 0}
    for i in range(96):
        volume = bytearray(rng.randbytes(256))
        transform = bytearray(rng.randbytes(128))
        initial = rng.randbytes(256)
        for m in [volume, transform]:
            for k, row in enumerate([(1, 0, 0, 0), (0, 1, 0, 0), (0, 0, 1, 0), (0, 0, 0, 1)]):
                struct.pack_into('>4f', m, 16 * k, *row)
        struct.pack_into('>4f', volume, 68, 2.0, 1.25, 1.5, 0.2)
        start = [rng.uniform(-5, 5) for k in range(4)]
        end = [rng.uniform(-5, 5) for k in range(4)]
        p1 = struct.pack('>4f', *start)
        p2 = struct.pack('>4f', *end)
        res = []
        for compiled in (False, True):
            c = F.cpu(b)
            c.__class__ = RunCPU
            c.rest_stubs = set()
            for addr, img in [(1342177280, volume), (1342181376, p1), (1342185472, p2), (1342189568, transform), (1342193664, initial)]:
                c.mem.add(addr, len(img), img)
            for a, n in b.unresolved.items():
                if n.startswith('__save'):
                    c.hooks[a] = lambda cpu: None
                elif n.startswith('__rest'):
                    c.rest_stubs.add(a)
                    c.hooks[a] = lambda cpu: None
            c.setg(3, 1342177280)
            c.setg(4, 1342181376)
            c.setg(5, 1342185472)
            c.setg(6, 1342189568 if i & 1 else 0)
            c.setg(7, 1342193664)
            c.setf(1, 0.15)
            try:
                c.run(b.entry if compiled else orig)
            except Exception as ex:
                fail.append({'function': name, 'case': i, 'compiled': compiled, 'error': repr(ex)})
                break
            res.append((c.g(3) & 4294967295, c.mem.rd(1342193664, 256), c.mem.rd(1342177280, 256)))
        if len(res) < 2:
            continue
        counts[name]['hit' if res[0][0] else 'miss'] += 1
        if res[0] != res[1]:
            diffs = [{'offset': k, 'original': res[0][1][k:k + 4].hex(), 'compiled': res[1][1][k:k + 4].hex()} for k in range(0, 256, 4) if res[0][1][k:k + 4] != res[1][1][k:k + 4]]
            bad.append({'function': name, 'case': i, 'orig': res[0][0], 'new': res[1][0], 'diffs': diffs[:8]})
    print(name, counts[name], 'mismatch', sum((x['function'] == name for x in bad)), 'fail', sum((x['function'] == name for x in fail)), flush=True)
print('bad', bad[:2], 'fail', fail[:2])
_r5_output(out / 'lineseg-simple-record-v2-final-selected-comparison.json').write_text(json.dumps({'counts': counts, 'mismatches': bad, 'errors': fail, 'objects': {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in paths}, 'scope': 'whole actual LineSeg providers vs original, finite distinct vectors, identity/null transforms; no full parity claim; failures and unsupported emulator retained'}, indent=2))