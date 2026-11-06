from pathlib import Path
import sys, json, struct, random, hashlib
P = Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow')
sys.path.insert(0, str(P / 'scratch/x360_sdk'))
import native_atomic_fixture as F
D = Path('D:/Reverse/BP-X360-Experimental-Artifacts/bootlegal-final-caller')
j = json.loads(Path('D:\\Reverse\\BP-X360-Experimental-Artifacts\\wave8\\repair-r6\\current-object-witnesses\\gui-stage\\actual-row-selection.json').read_text())
obj = Path(next((x for x in j['results'] if x['source'].endswith('BrnBootLegal.cpp')))['object'])
sym = '?Update@BootLegal@BrnGui@@'
o = F.Coff(obj, sym)
b = F.Bundle([o])
b.select(sym)
for i in range(48):
    stage = [2, 3, 4][i % 3]
    hd = i >> 2 & 1
    sku = i >> 3 & 1
    flag = i >> 4 & 1
    wait = i >> 5 & 1
    outs = []
    for native in (False, True):
        c = F.cpu(b)
        base = 1358954496
        queue = 1375731712
        iface = 1392508928
        cache = 1409286144
        for addr, size in [(base, 16384), (queue, 4096), (iface, 4096), (cache, 131072)]:
            c.mem.add(addr, size)
        F.put(c, base + 24, queue)
        F.put(c, base + 28, iface)
        F.put(c, base + 56, cache)
        F.put(c, base + 72, stage)
        F.put(c, base + 76, struct.unpack('>I', struct.pack('>f', 10.0))[0])
        c.mem.wr(base + 68, bytes([wait]))
        c.mem.wr(base + 80, bytes([flag]))
        c.mem.wr(cache + 19273, bytes([hd]))
        F.put(c, cache + 42996, sku)
        c.mem.wr(cache + 77578, bytes([flag, 0]))
        trace = []

        def first(x):
            F.put(x, x.g(4), 0)
            x.setg(3, -1)

        def clear(x):
            trace.append(('clear', x.g(3) & 4294967295))

        def gettime(x):
            trace.append(('time', x.g(3) & 4294967295))
            x.setf(1, 17.25)

        def ready(x):
            trace.append(('ready', x.g(3) & 4294967295, x.g(4)))
            x.setg(3, 1)

        def view(x):
            trace.append(('view', x.g(3) & 4294967295, F.cstring(x, x.g(4) & 4294967295), F.cstring(x, x.g(5) & 4294967295), x.g(6)))

        def output(x):
            x.setg(3, x.g(3) + 12 & 4294967295)

        def add(x):
            trace.append(('add', x.g(3) & 4294967295, x.g(5), x.g(6), x.mem.rd(x.g(4) & 4294967295, x.g(6))))
        for addr, fn in [(2184581080, first), (2184574064, clear), (2185294000, gettime), (2186209192, ready), (2189802312, view), (2185449864, add)]:
            c.hooks[addr] = fn
        for n, v, sec in o.symbols.values():
            if (0, sec) not in b.bases:
                continue
            addr = b.bases[0, sec] + v
            if '?GetFirstEvent@?$VariableEventQueue@$0EIAA@' in n:
                c.hooks[addr] = first
            elif '?Clear@?$VariableEventQueue@$0EIAA@' in n:
                c.hooks[addr] = clear
            elif '?AddEvent@?$VariableEventQueue@$0BAAAA@' in n:
                c.hooks[addr] = add
        for addr, n in b.unresolved.items():
            if '?GetTime@GuiCache@' in n:
                c.hooks[addr] = gettime
            elif '?AreAllAptComponentsInitialised@GuiCache@' in n:
                c.hooks[addr] = ready
            elif '?AddOutputAptViewState@GuiComponent@' in n:
                c.hooks[addr] = view
            elif '?GetOutputEventQueue@StateInterface@' in n:
                c.hooks[addr] = output
        c.setg(3, base)
        try:
            c.run(b.entry if native else 2185720432)
        except Exception:
            print('failed', i, 'native', native, 'pc', hex(c.last_pc))
            raise
        outs.append((c.mem.rd(base + 72, 16), trace))
    if outs[0] != outs[1]:
        print('MISMATCH', i, 'stage', stage, 'hd,sku,flag,wait', hd, sku, flag, wait, outs)
        break
else:
    out = {'passed': True, 'cases': 48, 'object_sha256': hashlib.sha256(obj.read_bytes()).hexdigest(), 'scope': 'Actual whole BootLegal::Update vs ARTIST 82477270 with no queued input events, cases 2/3/4 covering HD/SKU/overlay/sign-in/wait flags and time. Compare stage and timer bytes, call receivers, component strings and controlled queue/service traces; modeled CPU, no full lifecycle or runtime claim.'}
    _r5_output(D / 'boot-stage-comparison.json').write_text(json.dumps(out, indent=2) + '\n')
    print(out)