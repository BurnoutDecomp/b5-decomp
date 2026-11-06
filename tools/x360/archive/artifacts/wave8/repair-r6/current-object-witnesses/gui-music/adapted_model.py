from pathlib import Path
import sys, json, hashlib, random
P = Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow')
sys.path.insert(0, str(P / 'scratch/x360_sdk'))
import native_atomic_fixture as F
D = Path('D:/Reverse/BP-X360-Experimental-Artifacts/bootlegal-overlay-retention')
j = json.loads(Path('D:\\Reverse\\BP-X360-Experimental-Artifacts\\wave8\\repair-r6\\current-object-witnesses\\gui-music\\actual-row-selection.json').read_text())
obj = Path(next((x for x in j['results'] if 'OutputGuiEvent_Inst.cpp' in x['source']))['object'])
sym = '??$OutputGuiEvent@VGuiEventPlayMusicOnMenuStream@CgsGui@@@StateInterface@CgsGui@@'
o = F.Coff(obj, sym)
b = F.Bundle([o])
b.select(sym)
rng = random.Random(82476)
for i in range(128):
    payload = bytes((rng.getrandbits(8) for _ in range(8)))
    initial = bytes((rng.getrandbits(8) for _ in range(20)))
    outs = []
    for native in (False, True):
        c = F.cpu(b)
        iface = F.M.S.SELF
        event = F.M.S.AUX
        sp = c.g(1)
        c.mem.wr(event, payload)
        c.mem.wr(sp - 112 + 80, initial)
        trace = []

        def hook(x):
            trace.append((x.g(3) & 4294967295, x.g(5), x.g(6), x.mem.rd(x.g(4) & 4294967295, x.g(6))))
        c.hooks[2185449864] = hook
        for n, v, sec in o.symbols.values():
            if '?AddEvent@?$VariableEventQueue@$0BAAAA@$0BA@@' in n and (0, sec) in b.bases:
                c.hooks[b.bases[0, sec] + v] = hook
        c.setg(3, iface)
        c.setg(4, event)
        c.run(b.entry if native else 2185718784)
        outs.append(trace)
    assert outs[0] == outs[1], (i, outs)
out = {'passed': True, 'cases': 128, 'object_sha256': hashlib.sha256(obj.read_bytes()).hexdigest(), 'scope': 'Actual whole music OutputGuiEvent vs ARTIST 82476C00 with randomized payload and initialized 20-byte record; compares receiver, channel, length and all bytes including untouched tail. AddEvent controlled, modeled CPU.'}
_r5_output(D / 'music-wrapper-comparison.json').write_text(json.dumps(out, indent=2) + '\n')
print(out)