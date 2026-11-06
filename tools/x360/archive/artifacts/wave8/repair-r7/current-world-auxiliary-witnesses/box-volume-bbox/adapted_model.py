import sys, random, json, struct, hashlib
from pathlib import Path
sys.path.insert(0, 'C:\\Users\\adri1\\.codex\\worktrees\\fbaf\\BP-Decomp_Workflow\\scratch\\x360_sdk')
import native_atomic_fixture as F
out = Path('D:/Reverse/BP-X360-Experimental-Artifacts/parallel/wave8_scene_terra/volume-baseline')
obj = _r5_object(out / 'BoxVolume-wip-v4.obj')
b = F.Bundle([F.Coff(obj, '?GetBBox@SphereVolume@')])
rng = random.Random(2193260576)
A = 1342177280
O = 1342181376
T = 1342185472
bad = []
for typ, addr in [('SphereVolume', 2193260576), ('BoxVolume', 2193268680)]:
    b.select('?GetBBox@' + typ + '@')
    for i in range(128):
        src = bytearray(rng.randbytes(256))
        tm = bytearray(rng.randbytes(256))
        initial = rng.randbytes(256)
        for j in list(range(0, 64, 4)) + [68, 72, 76, 80]:
            struct.pack_into('>f', src, j, rng.uniform(-100, 100))
        for j in range(0, 64, 4):
            struct.pack_into('>f', tm, j, rng.uniform(-5, 5))
        r = []
        for compiled in (False, True):
            c = F.cpu(b)
            c.mem.add(A, 256, src)
            c.mem.add(O, 256, initial)
            c.mem.add(T, 256, tm)
            c.setg(3, A)
            c.setg(4, T if i & 1 else 0)
            c.setg(5, i & 1)
            c.setg(6, O)
            c.run(b.entry if compiled else addr)
            r.append((c.mem.rd(O, 256), c.mem.rd(A, 256), c.mem.rd(T, 256), c.g(3) & 4294967295))
        if r[0] != r[1]:
            bad.append([typ, i, r[0][0][:32].hex(), r[1][0][:32].hex()])
print('cases 256 mismatches', len(bad), bad[:3])
assert not bad
_r5_output(out / 'bbox-whole-wip-v4-comparison.json').write_text(json.dumps({'cases': 256, 'mismatches': bad, 'object_sha256': hashlib.sha256(obj.read_bytes()).hexdigest(), 'scope': 'actual whole BoxVolume.cpp object WIPs, finite distinct XYZW, both null/non-null transforms, full AABB and guards; whole BoxVolume TU compiled, remaining descriptor closure open'}, indent=2))