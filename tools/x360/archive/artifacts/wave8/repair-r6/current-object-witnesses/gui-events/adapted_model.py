from pathlib import Path
import sys, json, hashlib, struct
P = Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow')
sys.path.insert(0, str(P / 'scratch/x360_sdk'))
import native_atomic_fixture as F
D = Path('D:/Reverse/BP-X360-Experimental-Artifacts/gui-event-provider-final')
j = json.loads(Path('D:\\Reverse\\BP-X360-Experimental-Artifacts\\wave8\\repair-r6\\current-object-witnesses\\gui-events\\actual-row-selection.json').read_text())
obj = Path(next((x for x in j['results'] if x['source'].endswith('BrnBootLegal.cpp')))['object'])
sym = '?Update@BootLegal@BrnGui@@'
o = F.Coff(obj, sym)
b = F.Bundle([o])
b.select(sym)
h = 1311768467463790320
for i in range(96):
    eventid = [189, 190, 567][i % 3]
    match = i // 3 % 2
    leave = i // 6 % 2
    outs = []
    for native in (False, True):
        c = F.cpu(b)
        base = 1358954496
        queue = 1375731712
        iface = 1392508928
        cache = 1409286144
        event = 1426063360
        for a, sz in [(base, 16384), (queue, 4096), (iface, 4096), (cache, 131072), (event, 4096)]:
            c.mem.add(a, sz)
        for a, v in [(base + 24, queue), (base + 28, iface), (base + 56, cache), (base + 72, 10), (cache + 4, 1099563008), (event + 8, leave)]:
            F.put(c, a, v)
        c.mem.wr(event, (h if match else h ^ 4294967296).to_bytes(8, 'big'))
        c.mem.wr(base + 68, b'\x00')
        c.mem.wr(base + 80, b'\x01')
        trace = []

        def first(x):
            F.put(x, x.g(4), event)
            x.setg(3, eventid)

        def nextfn(x):
            F.put(x, x.g(5), 0)
            x.setg(3, -1)

        def clear(x):
            trace.append(('clear', x.g(3) & 4294967295))

        def hashfn(x):
            trace.append(('hash', F.cstring(x, x.g(3) & 4294967295)))
            x.setg(3, h)

        def gettime(x):
            x.setf(1, 17.25)
        for a, f in [(2184581080, first), (2184619488, nextfn), (2184574064, clear), (2189515296, hashfn), (2185294000, gettime)]:
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
            if '?CgsIDCompress@@' in n:
                c.hooks[a] = hashfn
            elif '?GetTime@GuiCache@' in n:
                c.hooks[a] = gettime
        c.setg(3, base)
        try:
            c.run(b.entry if native else 2185720432)
        except Exception:
            print('fail', i, native, hex(c.last_pc))
            raise
        outs.append((trace, c.mem.rd(base + 64, 24)))
    assert outs[0] == outs[1], (i, outs)
out = {'passed': True, 'cases': 96, 'object_sha256': hashlib.sha256(obj.read_bytes()).hexdigest(), 'scope': 'Actual BootLegal Update overlay-opened190, overlay-complete189 and resource-ready567 against ARTIST with matching/nonmatching full64-bit IDs and leave-method variants; stage10 default, state bytes and queue traces. Hash, queue and native time getter controlled; modeled CPU.'}
_r5_output(D / 'boot-event-comparison.json').write_text(json.dumps(out, indent=2) + '\n')
print(out)