from pathlib import Path
import ast, re, json, struct, random, hashlib, sys, numpy as np
sys.path.insert(0, 'D:/Reverse/BP-Decomp_Workflow/scratch/CRASHPARITY_0922/fxcrashvfx_vmxemu')
import emu64 as E, vmxemu as V
p = Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/scratch/x360_sdk/validate_render_states.py')
t = ast.parse(p.read_text())
exec(compile(ast.Module(body=[n for n in t.body if isinstance(n, ast.ClassDef) and n.name == 'Coff'], type_ignores=[]), 'coff', 'exec'), globals())
HELP = 1073741824
ACTIVE = 1090519040
CAR = 1107296256
STATE = 1124073472
MOD = 1157627904
STACK = 1140850688
RNG = MOD + 181184

class CPU(E.CPU64):
    pass

def cpu():
    m = V.Mem()
    for a, n in [(HELP, 4096), (ACTIVE, 4096), (CAR, 4096), (STATE, 4096), (MOD, 196608), (STACK, 131072)]:
        m.add(a, n)
    c = CPU(m)
    c.setg(1, STACK + 65536)
    return c

class M:
    pass
M.new_cpu = staticmethod(cpu)
obj = _r5_object(Path('D:/Reverse/BP-X360-Experimental-Artifacts/effects-native-terra/gate-final3/JumpStateMachine-7a6f23f4.obj'))
coff = Coff(obj, '?FireWheelSparks@JumpStateMachine@')
rng = random.Random(3614)
errors = []
for i in range(128):
    contact = [int(i % 4 != 1), int(i % 4 != 2)]
    wheels = [[rng.uniform(-100, 100) for j in range(8)] for k in range(2)]
    seed = rng.getrandbits(64)
    cursor = i % 8
    buffer = [rng.uniform(1, 1.999) for j in range(8)]
    ground = rng.uniform(-20, 20)
    dt = 0.016
    time = rng.uniform(0, 100)
    outs = []
    for native in (False, True):
        c = coff.cpu() if native else cpu()
        c.mem.wr(HELP, struct.pack('>5I', 0, ACTIVE, MOD, CAR, 0))
        c.mem.wr(STATE + 64, struct.pack('>I', CAR))
        c.mem.wr(STATE + 16, struct.pack('>2f', dt, time))
        c.mem.wr(ACTIVE + 312, struct.pack('>f', ground))
        for off, vals, on in zip((224, 336), wheels, contact):
            c.mem.wr(CAR + off, struct.pack('>8f', *vals))
            c.mem.wr(CAR + off + 36, struct.pack('>I', 305397760 + off))
            c.mem.wr(CAR + off + 40, bytes([on]))
        c.mem.wr(RNG, struct.pack('>8f', *buffer))
        c.mem.wr(RNG + 32, struct.pack('>Q', seed))
        c.mem.wr(RNG + 40, struct.pack('>I', cursor))
        c.setg(3, 0)
        c.setg(4, STATE)
        c.setg(5, HELP)
        events = []

        def fire(q):
            events.append((struct.pack('>4I', *q.vr[1]).hex(), struct.pack('>4I', *q.vr[2]).hex(), *[struct.pack('>f', q.f(n)).hex() for n in (1, 2, 3)], q.g(6) & 4294967295, q.mem.rd(q.g(8) & 4294967295, 4).hex()))

        def rand(q):
            p = q.g(4) & 4294967295
            dest = q.g(3) & 4294967295
            cur = struct.unpack('>I', q.mem.rd(p + 40, 4))[0]
            slot = cur + 3 & 4
            x = struct.unpack('>f', q.mem.rd(p + slot * 4, 4))[0]
            old = struct.unpack('>Q', q.mem.rd(p + 32, 8))[0]
            new = old * 6364136223846793005 + 1 & 18446744073709551615
            bits = 1065353216 | old >> 32 >> 9
            q.mem.wr(p + 32, struct.pack('>Q', new))
            q.mem.wr(p + slot * 4, struct.pack('>I', bits))
            q.mem.wr(p + 40, struct.pack('>I', slot + 1))
            q.mem.wr(dest, struct.pack('>4f', *[float(np.float32(x - 1.0))] * 4))
        if native:
            for addr, name in coff.unresolved.items():
                if name.startswith('?RandomVecFloat@'):
                    c.hooks[addr] = rand
                elif name.startswith('?FireJumpSparks@'):
                    c.hooks[addr] = fire
                elif name.startswith('__savevmx_') or name.startswith('__restvmx_'):
                    c.hooks[addr] = lambda q: None
            c.run(coff.entry)
        else:
            c.hooks[2193662396] = lambda q: None
            c.hooks[2193663060] = lambda q: None
            c.hooks[2183752160] = fire
            c.run(2183763568)
        outs.append((events, c.mem.rd(RNG, 44).hex()))
    if outs[0] != outs[1]:
        errors.append({'case': i, 'original': outs[0], 'compiled': outs[1]})
r = {'cases': 128, 'mismatches': len(errors), 'examples': errors[:3], 'object_sha256': hashlib.sha256(obj.read_bytes()).hexdigest(), 'scope': 'actual compiled FireWheelSparks vs original 82299670 with controlled random provider reproducing original LCG/ring and FireJumpSparks captured; full RNG mutation+vector/FPR arguments, finite values and ground gates; no GPU/hardware proof'}
_r5_output(Path('D:/Reverse/BP-X360-Experimental-Artifacts/effects-native-terra/jump-sparks-comparison.json')).write_text(json.dumps(r, indent=2))
print(json.dumps(r, indent=2))
assert not errors