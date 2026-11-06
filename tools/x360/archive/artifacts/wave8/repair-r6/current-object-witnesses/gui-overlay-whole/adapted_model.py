from pathlib import Path
import sys, json, hashlib, random
P = Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow')
sys.path.insert(0, str(P / 'scratch/x360_sdk'))
import native_atomic_fixture as F
D = Path('D:/Reverse/BP-X360-Experimental-Artifacts/gui-event-provider-final')
j = json.loads(Path('D:\\Reverse\\BP-X360-Experimental-Artifacts\\wave8\\repair-r6\\current-object-witnesses\\gui-overlay-whole\\actual-row-selection.json').read_text())
obj = Path(j['results'][0]['object'])
rng = random.Random(2184912072)
cases = 0
for name, orig in [('?Construct@GuiOverlayRequest@BrnGui@@QAAXPBD@Z', 2184912072), ('?AddMessageParam@GuiOverlayRequest@BrnGui@@', 2185701912), ('?GetMessageParam@GuiOverlayRequest@BrnGui@@', 2186197320), ('?GetButton1Param@GuiOverlayRequest@BrnGui@@', 2186197624), ('?GetButton2Param@GuiOverlayRequest@BrnGui@@', 2186197816)]:
    o = F.Coff(obj, name)
    b = F.Bundle([o])
    b.select(name)
    for i in range(64):
        initial = bytearray((rng.getrandbits(8) for _ in range(288)))
        initial[280:284] = 1 .to_bytes(4, 'big')
        initial[284] = initial[285] = 1
        for k in [12, 80, 148, 216]:
            initial[k:k + 64] = bytes([65 + i % 20]) * 63 + b'\x00'
        text = bytes([97 + i % 20]) * min(i, 63) + b'\x00'
        outs = []
        for native in (False, True):
            c = F.cpu(b)
            base = F.M.S.SELF
            arg = F.M.S.AUX
            c.mem.wr(base, initial)
            c.mem.wr(arg, text + b'\x00' * 80)

            def hashfn(x):
                x.setg(3, 1234605616436508552)

            def sprint(x):
                dst = x.g(3) & 4294967295
                cap = x.g(4)
                textptr = x.g(6) & 4294967295
                raw = bytearray()
                while len(raw) < cap - 1:
                    ch = x.mem.rd(textptr + len(raw), 1)[0]
                    if ch == 0:
                        break
                    raw.append(ch)
                x.mem.wr(dst, bytes(raw) + b'\x00')
                x.setg(3, len(raw))
            c.hooks[2189515296] = hashfn
            c.hooks[2183075064] = sprint
            for a, n in b.unresolved.items():
                if 'CgsIDCompress' in n:
                    c.hooks[a] = hashfn
                elif 'SPrintf' in n:
                    c.hooks[a] = sprint
            c.setg(3, base)
            if 'Construct' in name:
                c.setg(4, arg)
            elif 'AddMessage' in name:
                c.setg(4, 2)
                c.setg(5, arg)
            elif 'GetMessage' in name:
                c.setg(4, arg)
                c.setg(5, 0)
            else:
                c.setg(4, arg)
            try:
                c.run(b.entry if native else orig)
            except Exception:
                print('fail', name, i, native, hex(c.last_pc))
                raise
            outs.append((c.mem.rd(base, 288), c.mem.rd(arg, 80)))
        assert outs[0] == outs[1], (name, i, outs)
        cases += 1
out = {'passed': True, 'cases': cases, 'object_sha256': hashlib.sha256(obj.read_bytes()).hexdigest(), 'scope': 'Five actual whole-provider object methods vs ARTIST valid-input methods, randomized full 288-byte request and output, 64 cases each including text lengths 0..63. Hash and SPrintf services controlled; no assertion-failure or full runtime claim.'}
_r5_output(D / 'overlay-method-comparison.json').write_text(json.dumps(out, indent=2) + '\n')
print(out)