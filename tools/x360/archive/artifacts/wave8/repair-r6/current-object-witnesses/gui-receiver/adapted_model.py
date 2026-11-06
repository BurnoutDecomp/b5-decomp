from pathlib import Path
import sys, json, hashlib, random
P = Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow')
sys.path.insert(0, str(P / 'scratch/x360_sdk'))
import native_atomic_fixture as F
D = Path('D:/Reverse/BP-X360-Experimental-Artifacts/bootlegal-overlay-retention')
j = json.loads(Path('D:\\Reverse\\BP-X360-Experimental-Artifacts\\wave8\\repair-r6\\current-object-witnesses\\gui-receiver\\actual-row-selection.json').read_text())
obj = Path(next((x for x in j['results'] if x['source'].endswith('BrnGuiMovieManager.cpp')))['object'])
sym = '?RecvEvent@MovieManager@BrnGui@@'
o = F.Coff(obj, sym)
b = F.Bundle([o])
b.select(sym)
rng = random.Random(82496)
for i in range(32):
    payload = bytes((rng.getrandbits(8) for _ in range(48)))
    outs = []
    for native in (False, True):
        c = F.cpu(b)
        base = 1358954496
        event = 1375731712
        c.mem.add(base, 16384)
        c.mem.add(event, 4096)
        c.mem.wr(event, payload)
        c.mem.wr(base + 5148, b'\x01')
        trace = []

        def copy(x):
            trace.append((x.g(3) & 4294967295, x.g(4) & 4294967295, x.mem.rd(x.g(4) & 4294967295, 48)))
        c.hooks[2186194904] = copy
        for n, v, sec in o.symbols.values():
            if '?Copy@VideoDefinition@MovieManager@BrnGui@@' in n and (0, sec) in b.bases:
                c.hooks[b.bases[0, sec] + v] = copy
        for a, n in b.unresolved.items():
            if '?Copy@VideoDefinition@MovieManager@BrnGui@@' in n:
                c.hooks[a] = copy
        c.setg(3, base)
        c.setg(4, event)
        c.setg(5, 508)
        c.run(b.entry if native else 2186253960)
        outs.append((trace, c.mem.rd(base + 5148, 1)))
    assert outs[0] == outs[1], (i, outs)
out = {'passed': True, 'cases': 32, 'object_sha256': hashlib.sha256(obj.read_bytes()).hexdigest(), 'scope': 'Actual MovieManager::RecvEvent 508 vs ARTIST 824F9688, same receiver/payload addresses, compares raw Copy receiver and source pointer/48-byte payload plus stop-flag store. Copy controlled; non-null event only, no other state/runtime claim.'}
_r5_output(D / 'video-receiver-comparison.json').write_text(json.dumps(out, indent=2) + '\n')
print(out)