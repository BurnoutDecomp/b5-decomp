import sys, random, struct, json, hashlib
from pathlib import Path
sys.path.insert(0, 'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/scratch/x360_sdk')
import native_atomic_fixture as F
out = Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave8/repair-r7/caller-repair-terra')
obj = _r5_object(out / 'Primitive-final.obj')
prefix = '?AddPrimitive@PrimitivePairListBuilder@CgsCollision@CgsSceneManager@@QAAXPBVVolume@'
b = F.Bundle([F.Coff(obj, prefix)])
b.select(prefix)
kinds = {1: ('Sphere', 2189509896, 16), 2: ('Capsule', 2189510144, 32), 4: ('Box', 2189510000, 80), 5: ('Cylinder', 2189510264, 80)}
rng = random.Random(2189511352)
bad = []
errors = []
counts = {}
for i in range(128):
    kind = (1, 2, 4, 5)[i % 4]
    name, orig, size = kinds[kind]
    vol = bytearray(rng.randbytes(96))
    struct.pack_into('>I', vol, 64, 1342189568)
    for off in (68, 72, 76, 80):
        struct.pack_into('>f', vol, off, rng.uniform(0.1, 3))
    desc = struct.pack('>I', kind) + bytes(36)
    mat = struct.pack('>16f', *(rng.uniform(-2, 2) for _ in range(16)))
    builder = rng.randbytes(128)
    ret = []
    for compiled in (False, True):
        c = F.cpu(b)
        c.mem.add(1342177280, 128, builder)
        c.mem.add(1342181376, 96, vol)
        c.mem.add(1342185472, 64, mat)
        c.mem.add(1342189568, 40, desc)
        trace = []
        c.rest = set()

        def leaf(cpu):
            trace.append(('leaf', cpu.g(3) & 4294967295, cpu.mem.rd(cpu.g(4) & 4294967295, size).hex(), cpu.f(1), cpu.g(6) & 4294967295))

        def boxset(cpu):
            trace.append(('boxset', cpu.mem.rd(cpu.g(4) & 4294967295, 64).hex(), tuple(cpu.vr[1]), tuple(cpu.vr[2])))
            cpu.mem.wr(cpu.g(3) & 4294967295, bytes(range(80)))
        leafprefix = '?AddPrimitive@PrimitivePairListBuilder@CgsCollision@CgsSceneManager@@QAAXPAU' + name + '@'
        addr = F.address(b, leafprefix) if compiled else orig
        c.hooks[addr] = leaf
        if kind == 4:
            c.hooks[F.address(b, '?Set@Box@CgsGeometric@@') if compiled else 2187225368] = boxset
        for a, n in b.unresolved.items():
            if n.startswith('__save'):
                c.hooks[a] = lambda cpu: None
            elif n.startswith('__rest'):
                c.rest.add(a)
                c.hooks[a] = lambda cpu: None
        c.setg(3, 1342177280)
        c.setg(4, 1342181376)
        c.setg(5, 1342185472)
        c.setg(7, 13124)
        c.setf(1, 0.125)
        try:
            c.run(b.entry if compiled else 2189511352)
        except Exception as ex:
            errors.append({'case': i, 'kind': kind, 'compiled': compiled, 'error': repr(ex)})
            break
        ret.append((trace, c.mem.rd(1342177280, 128), c.mem.rd(1342181376, 96), c.mem.rd(1342185472, 64)))
    if len(ret) == 2:
        counts[name] = counts.get(name, 0) + 1
        if ret[0] != ret[1]:
            bad.append({'case': i, 'kind': kind, 'original': ret[0][0], 'compiled': ret[1][0]})
print('counts', counts, 'mismatches', len(bad), 'errors', len(errors), bad[:1], errors[:2])
_r5_output(out / 'primitive-controlled-final.json').write_text(json.dumps({'counts': counts, 'mismatches': bad, 'errors': errors, 'object_sha256': hashlib.sha256(obj.read_bytes()).hexdigest(), 'scope': 'actual 82814AB8 vs compiled source; controlled shape append and Box::Set calls, complete shape payload/transform/volume/builder guards, random finite fields; no unsupported type or hardware claim'}, indent=2) + '\n')