from pathlib import Path
import sys, json, hashlib
P = Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow')
sys.path.insert(0, str(P / 'scratch/x360_sdk'))
import native_atomic_fixture as F
D = Path('D:/Reverse/BP-X360-Experimental-Artifacts/bootlegal-final-caller')
j = json.loads(Path('D:\\Reverse\\BP-X360-Experimental-Artifacts\\wave8\\repair-r6\\current-object-witnesses\\gui-leave\\actual-row-selection.json').read_text())
obj = Path(next((x for x in j['results'] if x['source'].endswith('BrnBootLegal.cpp')))['object'])
sym = '?OnLeave@BootLegal@BrnGui@@'
o = F.Coff(obj, sym)
b = F.Bundle([o])
b.select(sym)
for i in range(32):
    outs = []
    for native in (False, True):
        c = F.cpu(b)
        base = 1358954496
        iface = 1392508928
        cache = 1409286144
        vt = 1426063360
        call = 1442840576
        for addr, size in [(base, 16384), (iface, 4096), (cache, 4096), (vt, 4096)]:
            c.mem.add(addr, size)
        F.put(c, base + 28, iface)
        F.put(c, base + 56, cache if i % 2 else 0)
        F.put(c, base + 792, vt)
        F.put(c, vt + 24, call)
        trace = []

        def unreg(x):
            trace.append(('unreg', x.g(3), x.g(5), tuple((int.from_bytes(x.mem.rd((x.g(4) & 4294967295) + k * 4, 4), 'big') for k in range(7)))))

        def add(x):
            size = x.g(6)
            rec = x.mem.rd(x.g(4) & 4294967295, size)
            kind = int.from_bytes(rec[4:8], 'big')
            trace.append(('add', x.g(3), x.g(5), size, rec[:12] + (F.cstring(x, int.from_bytes(rec[12:16], 'big')) if kind == 18 else rec[12:]).encode() if kind == 18 else rec))

        def clear(x):
            trace.append(('menu-clear', x.g(3)))

        def unload(x):
            trace.append(('unload', x.g(3), x.g(5), x.mem.rd(x.g(4) & 4294967295, 8)))

        def watch(x):
            trace.append(('watch-clear', x.g(3), x.g(4)))

        def out(x):
            x.setg(3, x.g(3) + 12)
        for a, f in [(2189801728, unreg), (2185449864, add), (2186275688, unload), (2186208552, watch), (call, clear)]:
            c.hooks[a] = f
        for n, v, sec in o.symbols.values():
            if (0, sec) in b.bases and '?AddEvent@?$VariableEventQueue@$0BAAAA@' in n:
                c.hooks[b.bases[0, sec] + v] = add
        for a, n in b.unresolved.items():
            if '?UnRegisterForEvents@StateInterface@' in n:
                c.hooks[a] = unreg
            elif '?GetOutputEventQueue@StateInterface@' in n:
                c.hooks[a] = out
            elif '?UnloadResources@GuiCache@' in n:
                c.hooks[a] = unload
            elif '?ClearExpectedAptComponentList@GuiCache@' in n:
                c.hooks[a] = watch
        c.setg(3, base)
        try:
            c.run(b.entry if native else 2185723432)
        except Exception:
            print('fail', i, native, hex(c.last_pc))
            raise
        outs.append(trace)
    assert outs[0] == outs[1], (i, outs)
out = {'passed': True, 'cases': 32, 'object_sha256': hashlib.sha256(obj.read_bytes()).hexdigest(), 'scope': 'Actual whole BootLegal::OnLeave vs ARTIST 82477E28 with present and absent cache; checks order/receivers, registered IDs, commands (string value rather than relocated address), resource tuple and watcher flow. Queue/menu/cache services controlled; modeled CPU, no runtime claim.'}
_r5_output(D / 'boot-leave-comparison.json').write_text(json.dumps(out, indent=2) + '\n')
print(out)