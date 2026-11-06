from pathlib import Path
import sys, json, hashlib, random
P = Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow')
sys.path.insert(0, str(P / 'scratch/x360_sdk'))
import native_atomic_fixture as F
D = Path('D:/Reverse/BP-X360-Experimental-Artifacts/bootlegal-overlay-retention')
j = json.loads(Path('D:\\Reverse\\BP-X360-Experimental-Artifacts\\wave8\\repair-r6\\current-object-witnesses\\gui-copy\\actual-row-selection.json').read_text())
obj = Path(next((x for x in j['results'] if x['source'].endswith('BrnGuiMovieManager.cpp')))['object'])
sym = '?Copy@VideoDefinition@MovieManager@BrnGui@@'
o = F.Coff(obj, sym)
b = F.Bundle([o])
b.select(sym)
rng = random.Random(2186194904)
for i in range(128):
    src = bytes((rng.getrandbits(8) for _ in range(48)))
    dest = bytes((rng.getrandbits(8) for _ in range(48)))
    outs = []
    for native in (False, True):
        c = F.cpu(b)
        a = F.M.S.SELF
        t = F.M.S.AUX
        c.mem.wr(a, dest)
        c.mem.wr(t, src)
        c.setg(3, a)
        c.setg(4, t)
        c.run(b.entry if native else 2186194904)
        outs.append((c.mem.rd(a, 48), c.mem.rd(t, 48)))
    assert outs[0] == outs[1], (i, outs)
out = {'passed': True, 'cases': 128, 'object_sha256': hashlib.sha256(obj.read_bytes()).hexdigest(), 'scope': 'Actual whole VideoDefinition::Copy versus ARTIST 824EAFD8 on randomized 48-byte source/destination, compare complete destination and preserved source including untouched padding; valid source only, VMX emulator.'}
_r5_output(D / 'video-copy-comparison.json').write_text(json.dumps(out, indent=2) + '\n')
print(out)