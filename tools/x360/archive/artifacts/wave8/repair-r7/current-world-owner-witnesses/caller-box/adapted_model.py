import sys, random, struct, json, hashlib
from pathlib import Path
sys.path.insert(0, 'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/scratch/x360_sdk')
import native_atomic_fixture as F
out = Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave8/repair-r7/caller-repair-terra')
obj = _r5_object(out / 'AABBox.obj')
b = F.Bundle([F.Coff(obj, '?CreateFromBox@AABBoxBuilder@')])
b.select('?CreateFromBox@AABBoxBuilder@')
rng = random.Random(2192932320)
bad = []
errors = []
dirs = struct.pack('>12f', 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0)
for i in range(64):
    gp = bytearray(rng.randbytes(192))
    struct.pack_into('>I', gp, 172, 1342210048)
    bbox = rng.randbytes(64)
    interval = bytearray(rng.randbytes(144))
    for j in range(3):
        for off in (0, 16):
            struct.pack_into('>4f', interval, j * 48 + off, *(rng.uniform(-20, 20) for _ in range(4)))
    results = []
    for compiled in (False, True):
        c = F.cpu(b)
        c.mem.add(1342177280, len(gp), gp)
        c.mem.add(1342181376, len(bbox), bbox)
        c.mem.add(2200375168, 64, dirs + struct.pack('>I', 1) + bytes(12))
        trace = []

        def intervals(cpu):
            rr = cpu.g(3) & 4294967295
            pp = cpu.g(4) & 4294967295
            nn = cpu.g(5) & 4294967295
            oo = cpu.g(6) & 4294967295
            trace.append((rr, nn, cpu.mem.rd(pp, 48).hex()))
            cpu.mem.wr(oo, interval)
        c.hooks[1342210048] = intervals
        for a, n in b.unresolved.items():
            if n.startswith('__save'):
                c.hooks[a] = lambda cpu: None
        c.setg(3, 1342181376)
        c.setg(4, 1342177280)
        try:
            c.run(b.entry if compiled else 2192932320)
        except Exception as ex:
            errors.append({'case': i, 'compiled': compiled, 'error': repr(ex)})
            break
        results.append((c.mem.rd(1342181376, 64), c.mem.rd(1342177280, 192), trace))
    if len(results) == 2 and results[0] != results[1]:
        bad.append({'case': i, 'original': results[0][0].hex(), 'compiled': results[1][0].hex(), 'trace_orig': results[0][2], 'trace_new': results[1][2]})
print('box', 64, 'bad', len(bad), 'errors', len(errors), bad[:1], errors[:2])
_r5_output(out / 'box-controlled-v1.json').write_text(json.dumps({'cases': 64, 'mismatches': bad, 'errors': errors, 'object_sha256': hashlib.sha256(obj.read_bytes()).hexdigest(), 'scope': 'original 82B57DE0 vs actual object; controlled member interval provider, distinct nonbroadcast lanes, directions/count/receiver and complete bbox/GP guards; finite values and no hardware claim'}, indent=2) + '\n')