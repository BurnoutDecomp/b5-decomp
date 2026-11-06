import sys, random, struct, json, hashlib
from pathlib import Path
sys.path.insert(0, 'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/scratch/x360_sdk')
import native_atomic_fixture as F
out = Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave8/repair-r7/caller-repair-terra')
obj = _r5_object(out / 'AABBox.obj')
b = F.Bundle([F.Coff(obj, '?CreateFromTriangle@AABBoxBuilder@')])
b.select('?CreateFromTriangle@AABBoxBuilder@')
rng = random.Random(2192932256)
bad = []
errors = []
for i in range(64):
    gp = bytearray(rng.randbytes(192))
    bbox = rng.randbytes(64)
    for off in (0, 32, 48):
        struct.pack_into('>4f', gp, off, *(rng.uniform(-20, 20) for _ in range(4)))
    results = []
    for compiled in (False, True):
        c = F.cpu(b)
        c.mem.add(1342177280, len(gp), gp)
        c.setg(3, 1342177280)
        c.setg(4, 1342177280)
        try:
            c.run(b.entry if compiled else 2192932256)
        except Exception as ex:
            errors.append({'case': i, 'compiled': compiled, 'error': repr(ex)})
            break
        results.append(c.mem.rd(1342177280, 192))
    if len(results) == 2 and results[0] != results[1]:
        bad.append(i)
print('triangle alias', 64, 'bad', len(bad), 'errors', len(errors), bad[:5], errors[:2])
_r5_output(out / 'triangle-alias-v1.json').write_text(json.dumps({'cases': 64, 'mismatches': bad, 'errors': errors, 'object_sha256': hashlib.sha256(obj.read_bytes()).hexdigest(), 'scope': 'original 82B57DA0 vs whole compiled object, finite inputs, all four lanes and full bbox/GP guards; bbox aliases GP at offset zero, no hardware claim'}, indent=2) + '\n')