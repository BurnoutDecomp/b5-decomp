import sys, random, json, struct, hashlib
from pathlib import Path
sys.path.insert(0, 'C:\\Users\\adri1\\.codex\\worktrees\\fbaf\\BP-Decomp_Workflow\\scratch\\x360_sdk')
import native_atomic_fixture as F
sys.path.insert(0, 'D:\\Reverse\\BP-Decomp_Workflow\\tools\\re')
import x360rd
out = Path('D:/Reverse/BP-X360-Experimental-Artifacts/parallel/wave8_scene_terra/volume-baseline')
obj = _r5_object(out / 'BoxVolume-wip-v4.obj')
b = F.Bundle([F.Coff(obj, '??0BoxVolume@')])
rng = random.Random(2193268976)
A = 1342177280
R = 1342181376
bad = []
for typ, prefix, addr in [('ctor', '??0BoxVolume@', 2193268976), ('boxinit', '?Initialize@BoxVolume@', 2193269128), ('sphereinit', '?Initialize@SphereVolume@', 2193261800)]:
    b.select(prefix)
    for i in range(128):
        src = rng.randbytes(256)
        tab = rng.randbytes(28)
        resource = bytearray(rng.randbytes(64))
        ptr = A if typ == 'ctor' or i % 4 else 0
        struct.pack_into('>I', resource, 0, ptr)
        radius = rng.uniform(-100, 100)
        dims = tuple((struct.unpack('>I', struct.pack('>f', rng.uniform(-100, 100)))[0] for _ in range(4)))
        r = []
        for compiled in (False, True):
            c = F.cpu(b)
            c.mem.add(A, 256, src)
            c.mem.add(R, 64, resource)
            if compiled:
                tbl = next((a for a, n in b.unresolved.items() if 'gVolumeVTable' in n))
            else:
                c.mem.add(2200432352, 64)
                c.mem.add(2182615040, 4096, x360rd.rd(2182615040, 4096))
                tbl = 2200432352
            c.mem.wr(tbl, tab)
            c.setg(3, A if typ == 'ctor' else R)
            c.vr[1] = dims
            c.setf(1, radius)
            c.run(b.entry if compiled else addr)
            r.append((c.mem.rd(A, 256), c.mem.rd(R, 64), c.g(3) & 4294967295))
        if r[0] != r[1]:
            bad.append([typ, i, r[0][0].hex(), r[1][0].hex(), r[0][2], r[1][2]])
print('cases 384 mismatches', len(bad), bad[:2])
assert not bad
_r5_output(out / 'volume-ctor-whole-wip-v4-comparison.json').write_text(json.dumps({'cases': 384, 'mismatches': bad, 'object_sha256': hashlib.sha256(obj.read_bytes()).hexdigest(), 'scope': 'actual whole BoxVolume CPP: Box constructor and Box/Sphere Initialize, null/non-null resource, controlled descriptor table, finite vector/radius, entire object and guards'}, indent=2))