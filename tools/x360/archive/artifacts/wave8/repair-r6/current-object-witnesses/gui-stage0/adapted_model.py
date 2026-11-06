from pathlib import Path
import sys, json, hashlib
P = Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow')
sys.path.insert(0, str(P / 'scratch/x360_sdk'))
import native_atomic_fixture as F
D = Path('D:/Reverse/BP-X360-Experimental-Artifacts/bootlegal-final-caller')
j = json.loads(Path('D:\\Reverse\\BP-X360-Experimental-Artifacts\\wave8\\repair-r6\\current-object-witnesses\\gui-stage0\\actual-row-selection.json').read_text())
obj = Path(next((x for x in j['results'] if x['source'].endswith('BrnBootLegal.cpp')))['object'])
sym = '?Update@BootLegal@BrnGui@@'
o = F.Coff(obj, sym)
b = F.Bundle([o])
b.select(sym)
for i in range(32):
    outs = []
    loaded = i % 2
    for native in (False, True):
        c = F.cpu(b)
        base = 1358954496
        queue = 1375731712
        iface = 1392508928
        cache = 1409286144
        event = 1426063360
        for a, sz in [(base, 16384), (queue, 4096), (iface, 4096), (cache, 131072), (event, 4096)]:
            c.mem.add(a, sz)
        for a, v in [(base + 24, queue), (base + 28, iface), (base + 56, 0), (base + 72, 0), (event, cache)]:
            F.put(c, a, v)
        trace = []

        def first(x):
            F.put(x, x.g(4), event)
            x.setg(3, 64)

        def nextfn(x):
            F.put(x, x.g(5), 0)
            x.setg(3, -1)

        def clear(x):
            trace.append(('clear', x.g(3) & 4294967295))

        def resources(x):
            trace.append(('resources', x.g(3) & 4294967295, x.g(5), x.mem.rd(x.g(4) & 4294967295, 8)))
            x.setg(3, loaded)

        def resource(x):
            trace.append(('resource', x.g(3) & 4294967295, x.mem.rd(x.g(4) & 4294967295, 8)))
            x.setg(3, 1)
        for a, f in [(2184581080, first), (2184619488, nextfn), (2184574064, clear), (2186275672, resources), (2186275664, resource)]:
            c.hooks[a] = f
        for n, v, sec in o.symbols.values():
            if (0, sec) not in b.bases:
                continue
            a = b.bases[0, sec] + v
            if '?GetFirstEvent@?$VariableEventQueue@$0EIAA@' in n:
                c.hooks[a] = first
            elif '?GetNextEvent@?$VariableEventQueue@$0EIAA@' in n:
                c.hooks[a] = nextfn
            elif '?Clear@?$VariableEventQueue@$0EIAA@' in n:
                c.hooks[a] = clear
        for a, n in b.unresolved.items():
            if '?EnsureResourcesAreLoaded@GuiCache@' in n:
                c.hooks[a] = resources
            elif '?EnsureResourceIsLoaded@GuiCache@' in n:
                c.hooks[a] = resource
        c.setg(3, base)
        try:
            c.run(b.entry if native else 2185720432)
        except Exception:
            print('fail', i, native, hex(c.last_pc))
            raise
        outs.append((trace, c.mem.rd(base + 56, 4), c.mem.rd(base + 72, 4)))
    assert outs[0] == outs[1], (i, outs)
out = {'passed': True, 'cases': 32, 'object_sha256': hashlib.sha256(obj.read_bytes()).hexdigest(), 'scope': 'Actual whole BootLegal::Update stage0 receiving cache event vs ARTIST 82477270, loaded/unloaded resource outcomes; compares raw cache receiver, resource tuples, queue clear and state writes. Queue/cache services controlled, modeled CPU.'}
_r5_output(D / 'boot-stage0-comparison.json').write_text(json.dumps(out, indent=2) + '\n')
print(out)