from pathlib import Path
import sys, json, hashlib
P = Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow')
sys.path.insert(0, str(P / 'scratch/x360_sdk'))
import native_atomic_fixture as F
D = Path('D:/Reverse/BP-X360-Experimental-Artifacts/bootlegal-final-caller')
j = json.loads(Path('D:\\Reverse\\BP-X360-Experimental-Artifacts\\wave8\\repair-r6\\current-object-witnesses\\gui-stage1\\actual-row-selection.json').read_text())
obj = Path(next((x for x in j['results'] if x['source'].endswith('BrnBootLegal.cpp')))['object'])
sym = '?Update@BootLegal@BrnGui@@'
o = F.Coff(obj, sym)
b = F.Bundle([o])
b.select(sym)
for i in range(32):
    outs = []
    soft = i % 2
    for native in (False, True):
        c = F.cpu(b)
        base = 1358954496
        queue = 1375731712
        iface = 1392508928
        cache = 1409286144
        game = 1459617792
        for addr, size in [(base, 16384), (queue, 4096), (iface, 4096), (cache, 131072), (game, 4096), (2197880832, 4096)]:
            c.mem.add(addr, size)
        for a, v in [(base + 24, queue), (base + 28, iface), (base + 56, cache), (base + 72, 1), (2197881552, game)]:
            F.put(c, a, v)
        for off, name in [(88, 'HDCompAnimator_mc'), (228, 'esrb_anim'), (368, 'StartMessageAnimatorComponent'), (508, 'BackgroundAnimatorComponent'), (648, 'SelectionMenuAnimatorComponent')]:
            c.mem.wr(base + off + 4, name.encode() + b'\x00')
        trace = []

        def first(x):
            F.put(x, x.g(4), 0)
            x.setg(3, -1)

        def clear(x):
            trace.append(('queue-clear', x.g(3) & 4294967295))

        def watchclear(x):
            trace.append(('watch-clear', x.g(3) & 4294967295, x.g(4)))

        def watchappend(x):
            trace.append(('watch-add', x.g(3) & 4294967295, x.g(4), F.cstring(x, x.g(5) & 4294967295)))

        def add(x):
            size = x.g(6)
            rec = x.mem.rd(x.g(4) & 4294967295, size)
            trace.append(('add', x.g(3) & 4294967295, x.g(5), size, rec[:13] if rec[4:8] == b'\x00\x00\x00X' else rec[:12]))

        def out(x):
            x.setg(3, x.g(3) + 12 & 4294967295)

        def apt(x):
            trace.append(('apt', x.g(3) & 4294967295, F.cstring(x, x.g(4) & 4294967295), x.g(5)))

        def h(x):
            trace.append(('hash', F.cstring(x, x.g(3) & 4294967295)))
            x.setg(3, 305419896)

        def music(x):
            trace.append(('music', x.g(3) & 4294967295, x.mem.rd(x.g(4) & 4294967295, 6)))

        def soften(x):
            trace.append(('soft', x.g(3) & 4294967295))
            x.setg(3, soft)

        def invite(x):
            trace.append(('invite', x.g(3) & 4294967295))
            x.setg(3, 0)

        def mute(x):
            data = x.mem.rd(x.g(4) & 4294967295, 1)
            trace.append(('add', x.g(3) + 12 & 4294967295, 40, 16, b'\x00\x00\x00\x01\x00\x00\x00X\x00\x00\x00\x0c' + data))
        for a, f in [(2184581080, first), (2184574064, clear), (2186208552, watchclear), (2186250176, watchappend), (2185449864, add), (2185457424, apt), (2187893328, h), (2185718784, music), (2184970872, soften), (2184970888, invite), (2185718704, mute)]:
            c.hooks[a] = f
        for n, v, sec in o.symbols.values():
            if (0, sec) not in b.bases:
                continue
            a = b.bases[0, sec] + v
            if '?GetFirstEvent@?$VariableEventQueue@$0EIAA@' in n:
                c.hooks[a] = first
            elif '?Clear@?$VariableEventQueue@$0EIAA@' in n:
                c.hooks[a] = clear
            elif '?AddEvent@?$VariableEventQueue@$0BAAAA@' in n:
                c.hooks[a] = add
        for a, n in b.unresolved.items():
            if '?gpBurnoutGame@@' in n:
                F.put(c, a, game)
            elif '?GetOutputEventQueue@StateInterface@' in n:
                c.hooks[a] = out
            elif '?ClearExpectedAptComponentList@GuiCache@' in n:
                c.hooks[a] = watchclear
            elif '?AppendExpectedAptComponent@GuiCache@' in n:
                c.hooks[a] = watchappend
            elif '?PlayAptMovie@StateInterface@' in n:
                c.hooks[a] = apt
            elif '?MakeHash@Name@Playback@CgsSound@@' in n:
                c.hooks[a] = h
            elif 'OutputGuiEvent@VGuiEventPlayMusicOnMenuStream' in n:
                c.hooks[a] = music
            elif '?HasGameBeenSoftRebooted@' in n:
                c.hooks[a] = soften
            elif '?HasGameBeenRebootedDueToInvite@' in n:
                c.hooks[a] = invite
        c.setg(3, base)
        try:
            c.run(b.entry if native else 2185720432)
        except Exception:
            print('fail', i, native, hex(c.last_pc))
            raise
        outs.append((trace, c.mem.rd(base + 72, 4)))
    assert outs[0] == outs[1], (i, outs)
out = {'passed': True, 'cases': 32, 'object_sha256': hashlib.sha256(obj.read_bytes()).hexdigest(), 'scope': 'Actual BootLegal::Update stage1 vs ARTIST 82477270 with empty input, cold and soft-reboot non-invite, compares real cache/component name arguments, queue/event trace, apt movie, music name/hash payload, game receiver and resulting stage. Queue, music, game and cache services controlled; initialized event bytes only.'}
_r5_output(D / 'boot-stage1-comparison.json').write_text(json.dumps(out, indent=2) + '\n')
print(out)