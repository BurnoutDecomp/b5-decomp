import sys, random, json, struct, hashlib
from pathlib import Path
sys.path.insert(0, 'C:\\Users\\adri1\\.codex\\worktrees\\fbaf\\BP-Decomp_Workflow\\scratch\\x360_sdk')
import native_atomic_fixture as F
out = Path('D:/Reverse/BP-X360-Experimental-Artifacts/parallel/wave8_scene_terra/volume-baseline')
obj = _r5_object(out / 'PrimitiveIntersect-nan-wip.obj')
b = F.Bundle([F.Coff(obj, '?PrimitiveBatchIntersect@')])
b.select('?PrimitiveBatchIntersect@')
rng = random.Random(2193276024)
O = 1342177280
GP = 1342185472
GR = 1342189568
R1 = 1342193664
R2 = 1342197760
V1 = 1342201856
V2 = 1342205952
D = 1342210048
M = 1342214144
H = 2231369728
bad = []

def target(c, pc):
    w = c.fetch(pc)
    d = w & 67108860
    if d & 33554432:
        d -= 67108864
    return pc + d & 4294967295
for i in range(128):
    outbuf = rng.randbytes(4096)
    gp = rng.randbytes(512)
    group = bytearray(rng.randbytes(64))
    r1 = bytearray(rng.randbytes(128))
    r2 = bytearray(rng.randbytes(128))
    v1 = bytearray(rng.randbytes(256))
    v2 = bytearray(rng.randbytes(256))
    desc = bytearray(rng.randbytes(64))
    mtx = rng.randbytes(128)
    swapped = i & 1
    pad = 0.125
    struct.pack_into('>4I', group, 0, R1, 1, swapped, R2)
    struct.pack_into('>III', r1, 0, V1, M, 0)
    struct.pack_into('>I', r1, 112, 305419896)
    struct.pack_into('>III', r2, 0, V2, M, 0)
    struct.pack_into('>I', r2, 112, 2271560481)
    struct.pack_into('>I', v1, 64, D)
    struct.pack_into('>I', v2, 64, D)
    struct.pack_into('>I', desc, 20, H)
    r = []
    for compiled in (False, True):
        c = F.cpu(b)
        for addr, img in [(O, outbuf), (GP, gp), (GR, group), (R1, r1), (R2, r2), (V1, v1), (V2, v2), (D, desc), (M, mtx)]:
            c.mem.add(addr, len(img), img)
        trace = []

        def make(cpu):
            ptr = cpu.g(4) & 4294967295
            trace.append(('make', cpu.g(3) & 4294967295, ptr, cpu.g(5) & 4294967295))
            F.put(cpu, ptr + 132, 2695938256)
            cpu.setg(3, 1)
            cpu.setf(1, 77)

        def batch(cpu):
            trace.append(('batch', tuple((cpu.g(k) & 4294967295 for k in range(3, 8))), cpu.f(1)))
            cpu.setg(3, 0)
        c.hooks[H] = make
        if compiled:
            for fragment in ['?GPInstanceBatchIntersect1xN@', '?GPInstanceBatchIntersectNx1@']:
                c.hooks[F.address(b, fragment)] = batch
        else:
            for pc in [2193276260, 2193276280]:
                c.hooks[target(c, pc)] = batch
        c.setg(3, O)
        c.setg(4, 2)
        c.setg(5, GP)
        c.setg(6, GR)
        c.setg(7, 1)
        c.setf(1, pad)
        c.run(b.entry if compiled else 2193276024)
        r.append((c.mem.rd(GP, 512), c.mem.rd(O, 4096), trace, c.g(3) & 4294967295))
    if r[0] != r[1]:
        bad.append([i, r[0][2], r[1][2], r[0][0][:256].hex(), r[1][0][:256].hex()])
print('cases 128 mismatches', len(bad), bad[:1])
assert not bad
_r5_output(out / 'primitive-batch-whole-nan-v5-comparison.json').write_text(json.dumps({'cases': 128, 'mismatches': bad, 'object_sha256': hashlib.sha256(obj.read_bytes()).hexdigest(), 'scope': 'actual whole PrimitiveIntersect CPP PrimitiveBatchIntersect vs ARTIST 82BABC78, two real PMF slot +0x14 calls and both swapped batch targets controlled, preserves callback volumeTag and writes VolRef tag into userTag +0x88, padding f1 across calls, poisoned buffers; controlled services only'}, indent=2))