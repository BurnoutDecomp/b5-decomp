from pathlib import Path
import sys, json, hashlib, random
P = Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow')
sys.path.insert(0, str(P / 'scratch/x360_sdk'))
import native_atomic_fixture as F
D = Path('D:/Reverse/BP-X360-Experimental-Artifacts/bootlegal-final-caller')
j = json.loads(Path('D:\\Reverse\\BP-X360-Experimental-Artifacts\\wave8\\repair-r6\\current-object-witnesses\\gui-enter\\actual-row-selection.json').read_text())
obj = Path(next((x for x in j['results'] if x['source'].endswith('BrnBootLegal.cpp')))['object'])
sym = '?OnEnter@BootLegal@BrnGui@@'
o = F.Coff(obj, sym)
b = F.Bundle([o])
b.select(sym)
for i in range(32):
    outs = []
    for native in (False, True):
        c = F.cpu(b)
        base = 1358954496
        iface = 1392508928
        vt = 1426063360
        call = 1442840576
        game = 1459617792
        for addr, size in [(base, 16384), (iface, 4096), (vt, 4096), (game, 4096), (2197880832, 4096)]:
            c.mem.add(addr, size)
        F.put(c, base + 28, iface)
        F.put(c, 2197881552, game)
        F.put(c, vt, call)
        for off in (88, 228, 368, 508, 648):
            F.put(c, base + off, vt)
        trace = []

        def reg(x):
            trace.append(('reg', x.g(3), x.g(5), tuple((int.from_bytes(x.mem.rd((x.g(4) & 4294967295) + k * 4, 4), 'big') for k in range(7)))))

        def add(x):
            size = x.g(6)
            rec = x.mem.rd(x.g(4) & 4294967295, size)
            kind = int.from_bytes(rec[4:8], 'big')
            trace.append(('add', x.g(3), x.g(5), size, rec[:13] if kind == 214 else rec[:12] if kind == 20 else rec))

        def comp(x):
            trace.append(('component', x.g(3), F.cstring(x, x.g(4) & 4294967295), x.g(5), x.g(6)))

        def menu(x):
            trace.append(('menu', x.g(3), F.cstring(x, x.g(4) & 4294967295), x.g(5), x.g(6), x.g(7)))

        def soft(x):
            trace.append(('soft', x.g(3)))
            x.setg(3, 0)

        def out(x):
            x.setg(3, x.g(3) + 12)
        for a, f in [(2189801632, reg), (2185449864, add), (2186187552, menu), (2184970872, soft), (call, comp)]:
            c.hooks[a] = f
        for n, v, sec in o.symbols.values():
            if (0, sec) in b.bases and '?AddEvent@?$VariableEventQueue@$0BAAAA@' in n:
                c.hooks[b.bases[0, sec] + v] = add
        for a, n in b.unresolved.items():
            if '?gpBurnoutGame@@' in n:
                F.put(c, a, game)
            elif '?RegisterForEvents@StateInterface@' in n:
                c.hooks[a] = reg
            elif '?GetOutputEventQueue@StateInterface@' in n:
                c.hooks[a] = out
            elif '?Construct@MenuComponent@' in n:
                c.hooks[a] = menu
            elif '?HasGameBeenSoftRebooted@' in n:
                c.hooks[a] = soft
        c.setg(3, base)
        try:
            c.run(b.entry if native else 2185719848)
        except Exception:
            print('fail', i, native, hex(c.last_pc))
            raise
        outs.append((trace, c.mem.rd(base + 56, 32)))
    assert outs[0] == outs[1], (i, outs)
out = {'passed': True, 'cases': 32, 'object_sha256': hashlib.sha256(obj.read_bytes()).hexdigest(), 'scope': 'Actual whole BootLegal::OnEnter vs ARTIST 82477028 cold-boot branch, direct register/queue/component/menu/game receiver traces and state bytes. Queue/services and virtual component/menu constructors controlled; compares initialized record bytes, excludes untouched padding, soft-reboot branch and game/runtime claims.'}
_r5_output(D / 'boot-enter-comparison.json').write_text(json.dumps(out, indent=2) + '\n')
print(out)