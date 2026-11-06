import json, struct, sys, random, hashlib
import numpy as np
from pathlib import Path
sys.path.insert(0, 'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/scratch/x360_sdk')
import native_atomic_fixture as F

class RoadCPU(F.CPU):

    def exec_vector(self, a, w, mn):
        if w >> 26 == 6 and w & 2032 == 1648:
            d = w >> 21 & 31 | (w >> 2 & 3) << 5
            y = w >> 11 & 31 | (w & 3) << 5
            self.setvf(d, [np.float32(1.0 / np.sqrt(np.float64(x))) for x in self.vf(y)])
            return True
        if w >> 26 == 5 and w & 976 == 592:
            d = w >> 21 & 31 | (w >> 2 & 3) << 5
            x = w >> 16 & 31 | (w >> 5 & 1) << 5 | (w >> 10 & 1) << 6
            y = w >> 11 & 31 | (w & 3) << 5
            self.vr[d] = tuple((self.vr[x][i] & ~self.vr[y][i] & 4294967295 for i in range(4)))
            return True
        return super().exec_vector(a, w, mn)
p = Path('D:/Reverse/BP-X360-Experimental-Artifacts/roadrule-native-terra')
row = next((r for r in json.loads(Path('D:\\Reverse\\BP-X360-Experimental-Artifacts\\wave8\\repair-r7\\current-core-owner-witnesses\\road-distance\\actual-row-selection.json').read_text())['results'] if r['source'].endswith('BrnRoadRuleComponent.cpp')))
ob = F.Coff(Path(row['object']), '?UpdateRoadSignDistance@')
b = F.Bundle([ob])
b.select('?UpdateRoadSignDistance@')
key = next((v for k, v in b.definitions.items() if k.startswith('?GetSignOffsetSizeAdjustment@')))
offaddr = b.bases[key[0], key[2]] + key[1]
base = F.M.S.SELF
sign = F.M.S.AUX
bits = lambda f: struct.unpack('>I', struct.pack('>f', f))[0]

def go(original, cam, road, side, adjust, offset, active):
    c = b.cpu()
    c.__class__ = RoadCPU
    c.mem.add(2197487616, 65536)
    c.mem.add(2222981120, 4096)

    def store(a, vals):
        c.mem.wr(a, struct.pack('>4f', *vals))
    for a, val in [(2197499152, 50.0), (2197498352, 100.0), (2197498896, 0.01), (2197499120, 300.0), (2197498576, 50.0), (2197498528, 0.0), (2197498544, -160.0)]:
        store(a, [val] * 4)
    store(base + 464 + side * 16, road)
    c.wr32(sign + 20, 1 if active else 0)
    c.setg(3, base)
    c.setg(4, side)
    c.setg(5, sign)
    c.setvf(1, cam)
    c.setvf(2, [adjust] * 4)
    out = []

    def adjustment(q):
        store(q.g(3), [offset] * 4)

    def setpos(q):
        out.append(tuple(q.vr[1]))

    def ctor(q):
        val = q.mem.rd(q.g(4), 4)
        q.mem.wr(q.g(3), val * 4)
    for addr, name in b.unresolved.items():
        if name.startswith('??0VecFloat@'):
            c.hooks[addr] = ctor
        elif name.startswith('?SetPosition@'):
            c.hooks[addr] = setpos
        elif name.startswith('__savevmx_') or name.startswith('__restvmx_'):
            c.hooks[addr] = lambda q: None
    c.hooks[offaddr] = adjustment
    c.hooks[2185314552] = adjustment
    c.hooks[2185355200] = setpos
    c.run(2185408504 if original else b.entry)
    return out
rng = random.Random(3608242)
bad = []
for i in range(256):
    cam = [rng.uniform(-500, 500) for _ in range(4)]
    road = [rng.uniform(-500, 500) for _ in range(4)]
    side = i % 2
    adjust = (-1.0, 1.0)[side]
    offset = rng.choice([0.0, 4.0, -18.0, 6.5])
    active = i % 5 != 0
    a = go(True, cam, road, side, adjust, offset, active)
    z = go(False, cam, road, side, adjust, offset, active)
    if a != z:
        bad.append({'case': i, 'original': a, 'native': z})
for j, delta in enumerate([0.0, 0.0001, 0.000345266983, 0.0004, 49.0, 50.0, 150.0, 250.0]):
    cam = [delta, 99.0, 0.0, 0.0]
    road = [0.0, -5.0, 0.0, 0.0]
    a = go(True, cam, road, 0, 1.0, 4.0, True)
    z = go(False, cam, road, 0, 1.0, 4.0, True)
    if a != z:
        bad.append({'case': 'boundary' + str(j), 'original': a, 'native': z})
print('cases', 264, 'mismatches', len(bad), bad[:3])
report = {'cases': 264, 'mismatches': bad, 'object_sha256': hashlib.sha256(Path(row['object']).read_bytes()).hexdigest(), 'scope': 'full original and actual compiled method, same finite input, initialized data, controlled GetSignOffsetSizeAdjustment/SetPosition; all output lanes; modeled estimate, not hardware or complete runtime'}
_r5_output(p / 'distance-comparison-final.json').write_text(json.dumps(report, indent=2) + '\n')
assert not bad