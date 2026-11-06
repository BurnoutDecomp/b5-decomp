from pathlib import Path
import sys, json, hashlib
P = Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow')
sys.path.insert(0, str(P / 'scratch/x360_sdk'))
import native_atomic_fixture as F
D = Path('D:/Reverse/BP-X360-Experimental-Artifacts/gui-event-provider-final')
j = json.loads(Path('D:\\Reverse\\BP-X360-Experimental-Artifacts\\wave8\\repair-r6\\current-object-witnesses\\gui-stage7-8\\actual-row-selection.json').read_text())
obj = Path(next((x for x in j['results'] if x['source'].endswith('BrnBootLegal.cpp')))['object'])
sym = '?Update@BootLegal@BrnGui@@'
o = F.Coff(obj, sym)
b = F.Bundle([o])
b.select(sym)
for i in range(64):
    mode = i % 4
    stage = 7 if mode < 2 else 8
    eventid = 143 if mode == 0 else 6 if mode in (1, 2) else None
    available = i // 4 % 2
    selected = i // 8 % 2
    outs = []
    for native in (False, True):
        c = F.cpu(b)
        base = 1358954496
        queue = 1375731712
        iface = 1392508928
        cache = 1409286144
        event = 1426063360
        game = 1459617792
        global_orig = 2197792752
        for a, sz in [(base, 16384), (queue, 4096), (iface, 4096), (cache, 131072), (event, 4096), (game, 4096), (2197880832, 4096), (2197790720, 4096)]:
            c.mem.add(a, sz)
        for a, v in [(base + 24, queue), (base + 28, iface), (base + 56, cache), (base + 64, 0), (base + 72, stage), (event + 4, 49), (2197881552, game)]:
            F.put(c, a, v)
        c.mem.wr(base + 957, bytes([selected]))
        c.mem.wr(global_orig, bytes([available, 0]))
        native_global = next((a for a, n in b.unresolved.items() if '?g_DLCFeatureAvailability@' in n))
        c.mem.wr(native_global, bytes([available, 0]))
        trace = []

        def first(x):
            F.put(x, x.g(4), event if eventid is not None else 0)
            x.setg(3, eventid if eventid is not None else -1)

        def nextfn(x):
            F.put(x, x.g(5), 0)
            x.setg(3, -1)

        def clear(x):
            trace.append(('clear', x.g(3) & 4294967295))

        def menu(x):
            trace.append(('menu', x.g(3) & 4294967295))

        def invite(x):
            trace.append(('invite', x.g(3) & 4294967295))
            x.setg(3, 0)

        def gettime(x):
            trace.append(('time', x.g(3) & 4294967295))
            x.setf(1, 17.25)

        def out(x):
            x.setg(3, x.g(3) + 12 & 4294967295)

        def add(x):
            trace.append(('add', x.g(3) & 4294967295, x.g(5), x.g(6), x.mem.rd(x.g(4) & 4294967295, 12)))

        def ac(x):
            trace.append(('audio-construct', x.g(4), F.cstring(x, x.g(5) & 4294967295), F.cstring(x, x.g(6) & 4294967295), F.cstring(x, x.g(7) & 4294967295)))
            x.mem.wr(x.g(3) & 4294967295, bytes(range(100)))

        def audio(x):
            trace.append(('audio', x.g(3) & 4294967295, x.mem.rd(x.g(4) & 4294967295, 100)))

        def enable(x):
            addr = x.g(3) & 4294967295
            assert addr == (native_global if native else global_orig)
            trace.append(('enable', x.g(4)))
            x.mem.wr(addr + 1, bytes([x.g(4) & 255]))
        for a, f in [(2184581080, first), (2184619488, nextfn), (2184574064, clear), (2185714720, menu), (2184970888, invite), (2185294000, gettime), (2185449864, add), (2186240848, ac), (2185455760, audio), (2185702568, enable)]:
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
            elif '?AddEvent@?$VariableEventQueue@$0BAAAA@' in n:
                c.hooks[a] = add
            elif '?DisplaySelectionMenu@BootLegal@' in n:
                c.hooks[a] = menu
        for a, n in b.unresolved.items():
            if '?gpBurnoutGame@@' in n:
                F.put(c, a, game)
            elif '?HasGameBeenRebootedDueToInvite@' in n:
                c.hooks[a] = invite
            elif '?GetTime@GuiCache@' in n:
                c.hooks[a] = gettime
            elif '?GetOutputEventQueue@StateInterface@' in n:
                c.hooks[a] = out
            elif '?Construct@GuiAudioTriggerEvent@' in n:
                c.hooks[a] = ac
            elif 'OutputGuiEvent@UGuiAudioTriggerEvent' in n:
                c.hooks[a] = audio
            elif '?SetEnabledState@DLCBeatTheTeamGame@' in n:
                c.hooks[a] = enable
        c.setg(3, base)
        try:
            c.run(b.entry if native else 2185720432)
        except Exception:
            print('fail', i, native, hex(c.last_pc))
            raise
        glob = native_global if native else global_orig
        outs.append((trace, c.mem.rd(base + 64, 24), c.mem.rd(glob, 2)))
    assert outs[0] == outs[1], (i, outs)
out = {'passed': True, 'cases': 64, 'object_sha256': hashlib.sha256(obj.read_bytes()).hexdigest(), 'scope': 'Actual BootLegal Update stages7/8 vs ARTIST with start/accept/no input, DLC availability and selected-index combinations. Compares raw state/queue/audio traces and DLC bytes; global receiver identity checked against each image relocation, queue/menu/audio/game/DLC services controlled; modeled CPU, invite reboot excluded.'}
_r5_output(D / 'boot-stage7-8-comparison.json').write_text(json.dumps(out, indent=2) + '\n')
print(out)