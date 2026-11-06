from pathlib import Path
import sys, json, hashlib, random, struct
P = Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow')
sys.path.insert(0, str(P / 'scratch/x360_sdk'))
import native_atomic_fixture as F
D = Path('D:/Reverse/BP-X360-Experimental-Artifacts/gui-event-provider-final')
j = json.loads(Path('D:\\Reverse\\BP-X360-Experimental-Artifacts\\wave8\\repair-r6\\current-object-witnesses\\gui-worstcase\\actual-row-selection.json').read_text())
obj = Path(j['results'][0]['object'])
sym = '?DoWorstCase@GuiEventUpdateSatNav@BrnGui@@'
o = F.Coff(obj, sym)
b = F.Bundle([o])
b.select(sym)
loc = next((b.bases[0, s] + v for n, v, s in o.symbols.values() if n == 'sfWorstCasePhase' and (0, s) in b.bases))
rng = random.Random(2184911232)
for i in range(192):
    data = bytearray((rng.getrandbits(8) for _ in range(2308)))
    count = [0, 1, 2, 4, 8][i % 5]
    player = 0 if count == 0 else i // 5 % count
    for n in range(max(count, 1)):
        off = n * 48
        data[off:off + 16] = struct.pack('>4f', *(rng.uniform(-500, 500) for _ in range(4)))
        data[off + 36] = rng.randrange(6)
        data[off + 37] = rng.randrange(19)
        data[off + 40] = 0 if n == player else 2
    data[2304:2308] = count.to_bytes(4, 'big')
    phase = [0.0, 1.5, 99.0, 100.0, 101.0, -3.0][i % 6]
    outs = []
    for native in (False, True):
        c = F.cpu(b)
        base = 1358954496
        c.mem.add(base, 8192)
        c.mem.add(2197508096, 4096)
        c.mem.wr(base, data)
        p = struct.pack('>f', phase)
        c.mem.wr(2197508236, p)
        c.mem.wr(loc, p)
        c.setg(3, base)
        try:
            c.run(b.entry if native else 2184911232)
        except Exception:
            print('fail', i, native, hex(c.last_pc))
            raise
        outs.append((c.mem.rd(base, 2308), c.mem.rd(loc if native else 2197508236, 4)))
    if outs[0] != outs[1]:
        print('mismatch', i, 'count', count, 'player', player, 'phase', phase, 'diff', [(hex(n), a, z) for n, (a, z) in enumerate(zip(outs[0][0], outs[1][0])) if a != z][:20], outs[0][1], outs[1][1])
        raise SystemExit(1)
out = {'passed': True, 'cases': 192, 'object_sha256': hashlib.sha256(obj.read_bytes()).hexdigest(), 'scope': 'Actual whole GuiEventUpdateSatNav::DoWorstCase vs ARTIST 823B1980; randomized valid icon arrays, player indices, 0/1/2/4/8 counts and phase boundaries. Compare full 0x904 object and phase global, finite modeled VMX; valid/assert-success only, no complete HUD/runtime claim.'}
_r5_output(D / 'satnav-worstcase-comparison.json').write_text(json.dumps(out, indent=2) + '\n')
print(out)