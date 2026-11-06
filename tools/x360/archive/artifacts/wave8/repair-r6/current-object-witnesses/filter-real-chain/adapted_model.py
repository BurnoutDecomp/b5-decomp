import sys, json, random, struct, hashlib, math
from pathlib import Path
root = Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow')
sys.path.insert(0, str(root / 'scratch/x360_sdk'))
import native_atomic_fixture as F
out = Path('D:/Reverse/BP-X360-Experimental-Artifacts/audio-fma-r3-terra')
d = out / 'filter-successor'
q = _r5_object(d / 'Butterworth.obj')
errors = []
coverage = []
rng = random.Random(30432)
selfaddr = F.M.S.SELF
aux = F.M.S.AUX
mix = 1342177280
for name, orig in [('HighPassButterworth', 2193192672), ('LowPassButterworth', 2193193984)]:
    p = _r5_object(d / (name + '.obj'))
    prefix = '?Process@' + name + '@'
    b = F.Bundle([F.Coff(p, prefix), F.Coff(q, '?CalculateFilterCoefficients@Butterworth@')])
    b.select(prefix)
    for i in range(24):
        initial = bytearray(1024)
        initial[33] = 1 + i % 2
        struct.pack_into('>f', initial, 40, [1000.0, 0.0, 30000.0][i % 3])
        struct.pack_into('>f', initial, 48, float(1 + i % 4))
        struct.pack_into('>f', initial, 56, [0.5, 1.0, 2.0][i // 4 % 3])
        struct.pack_into('>H', initial, 88, 96)
        struct.pack_into('>I', initial, 136, 1 + i % 2)
        struct.pack_into('>H', initial, 140, 48)
        struct.pack_into('>H', initial, 142, 88)
        buf = bytearray(128)
        struct.pack_into('>I', buf, 4, aux + 32768)
        struct.pack_into('>H', buf, 14, 256)
        buf2 = bytearray(buf)
        struct.pack_into('>I', buf2, 4, aux + 49152)
        both = []
        for compiled in (False, True):
            c = F.cpu(b)
            c.mem.add(mix, 200704)
            c.mem.wr(selfaddr, initial)
            c.mem.wr(aux + 4096, buf)
            c.mem.wr(aux + 8192, buf2)
            c.mem.wr(aux + 32768, b''.join((struct.pack('>f', (j % 31 - 15) / 8.0) for j in range(512))))
            c.mem.wr(aux + 49152, bytes.fromhex('3f000000') * 512)
            F.put(c, aux + 20492, struct.unpack('>I', struct.pack('>f', 48000.0))[0])
            F.put(c, mix + 196620, aux + 4096)
            F.put(c, mix + 196624, aux + 8192)
            F.put(c, mix + 196632, aux + 20480)
            c.setg(3, selfaddr)
            c.setg(4, mix)
            c.setg(5, 0)
            trace = []
            provider = F.address(b, '?CalculateFilterCoefficients@Butterworth@') if compiled else 2192983704

            def observe(pc, word):
                if pc == provider:
                    trace.append(('designer', c.g(3) & 4294967295, c.g(5) & 4294967295, c.g(7) & 4294967295, struct.pack('>3f', c.f(1), c.f(2), c.f(3)).hex()))
                if pc == (F.address(b, '?Filter@Butterworth@') if compiled else 2192984552):
                    trace.append(('filter', c.g(3) & 4294967295, c.g(4) & 4294967295))
            c.instruction_observer = observe

            def memset(x):
                dst, val, n = [x.g(z) & 4294967295 for z in (3, 4, 5)]
                x.mem.wr(dst, bytes([val & 255]) * n)
                x.setg(3, dst)

            def tan(x):
                x.setf(1, math.tan(x.f(1)))

            def powf(x):
                x.setf(1, math.pow(x.f(1), x.f(2)))
            c.hooks[2190634960] = memset
            c.hooks[2193661552] = tan
            c.hooks[2193660272] = powf
            for at, n in b.unresolved.items():
                if n == 'memset':
                    c.hooks[at] = memset
                if n == 'tan':
                    c.hooks[at] = tan
                if n == 'pow':
                    c.hooks[at] = powf
                if n.startswith(('__savefpr_', '__restfpr_')):
                    c.hooks[at] = lambda x: None
            c.run(b.entry if compiled else orig)
            both.append((c.mem.rd(selfaddr, 1024), c.mem.rd(mix + 196608, 64), c.mem.rd(aux + 49152, 2048), trace))
        coverage.append({'name': name, 'case': i, 'channels': 1 + i % 2, 'order': 1 + i % 4, 'original_trace': both[0][3], 'compiled_trace': both[1][3]})
        if both[0] != both[1]:
            errors.append({'name': name, 'case': i, 'objdiff': [hex(j) for j, (a, z) in enumerate(zip(both[0][0], both[1][0])) if a != z][:30], 'dstdiff': sum((a != z for a, z in zip(both[0][2], both[1][2]))), 'trace': [both[0][3], both[1][3]]})
r = {'cases': 48, 'errors': errors, 'coverage': coverage, 'provider_sha256': hashlib.sha256(q.read_bytes()).hexdigest(), 'limits': 'Actual High/Low Processâ†’designerâ†’Filter, 1-2 channels, orders 1–4 and finite active/bypass inputs, tan/pow/memset/FPR helpers controlled; no hardware claim.'}
_r5_output(out / 'butterworth-filter-successor-chain-comparison.json').write_text(json.dumps(r, indent=2) + '\n')
print(len(errors), errors[:2])
assert not errors