from pathlib import Path
import sys, json, hashlib, random
P = Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow')
sys.path.insert(0, str(P / 'scratch/x360_sdk'))
import native_atomic_fixture as F
D = Path('D:/Reverse/BP-X360-Experimental-Artifacts/bootlegal-overlay-retention')
j = json.loads(Path('D:\\Reverse\\BP-X360-Experimental-Artifacts\\wave8\\repair-r6\\current-object-witnesses\\gui-prepare\\actual-row-selection.json').read_text())
obj = Path(next((x for x in j['results'] if x['source'].endswith('BrnGuiMovieManager.cpp')))['object'])
sym = '?Prepare@VideoDefinition@MovieManager@BrnGui@@'
o = F.Coff(obj, sym)
b = F.Bundle([o])
b.select(sym)
loc = next((b.bases[0, s] + v for n, v, s in o.symbols.values() if 'gsuDefaultVideoSoundName' in n and (0, s) in b.bases))
rng = random.Random(82472990)
for i in range(128):
    initial = bytes((rng.getrandbits(8) for _ in range(48)))
    default = rng.getrandbits(32)
    value = rng.getrandbits(32)
    outs = []
    for native in (False, True):
        c = F.cpu(b)
        base = F.M.S.SELF
        c.mem.wr(base, initial)
        c.mem.add(2197848064, 4096)
        F.put(c, 2197848744, default)
        F.put(c, loc, default)

        def h(x):
            x.setg(3, value)
        c.hooks[2190312616] = h
        for a, n in b.unresolved.items():
            if '?HashString@ID@CgsResource@@' in n:
                c.hooks[a] = h
        c.setg(3, base)
        c.run(b.entry if native else 2185701776)
        outs.append(c.mem.rd(base, 48))
    assert outs[0] == outs[1], (i, outs)
out = {'passed': True, 'cases': 128, 'object_sha256': hashlib.sha256(obj.read_bytes()).hexdigest(), 'scope': 'Actual whole VideoDefinition::Prepare vs ARTIST 82472990 with randomized prior48 bytes and controlled matching hash/default-sound inputs; compare all fields and untouched bytes, modeled VMX, HashString controlled.'}
_r5_output(D / 'video-prepare-comparison.json').write_text(json.dumps(out, indent=2) + '\n')
print(out)