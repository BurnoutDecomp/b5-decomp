from pathlib import Path
import ast, json, struct, hashlib, sys, re, math, numpy as np
sys.path.insert(0, 'D:/Reverse/BP-Decomp_Workflow/scratch/CRASHPARITY_0922/fxcrashvfx_vmxemu')
import emu64 as E, vmxemu as V
p = Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/scratch/x360_sdk/validate_render_states.py')
t = ast.parse(p.read_text())
exec(compile(ast.Module(body=[n for n in t.body if isinstance(n, ast.ClassDef) and n.name == 'Coff'], type_ignores=[]), 'coff', 'exec'), globals())
MOD = 1073741824
TEXT = 1090519040
DATA = 1107296256
STACK = 1140850688

class CPU(E.CPU64):
    pass

def cpu():
    m = V.Mem()
    for a, n in [(MOD, 262144), (TEXT, 4096), (DATA, 4096), (STACK, 131072)]:
        m.add(a, n)
    c = CPU(m)
    c.setg(1, STACK + 65536)
    return c

class M:
    pass
M.new_cpu = staticmethod(cpu)
obj = _r5_object(Path('D:/Reverse/BP-X360-Experimental-Artifacts/effects-native-terra/gate-final3/EffectsModule-87aee4b9.obj'))
coff = Coff(obj, '?JunkyardVfxStart@EffectsModule@BrnEffects@@')

def readz(q, addr, n=1024):
    out = bytearray()
    for i in range(n):
        b = q.mem.rd(addr + i & 4294967295, 1)[0]
        if not b:
            break
        out.append(b)
    return bytes(out)

def nicmp(q):
    a = q.g(3) & 4294967295
    b = q.g(4) & 4294967295
    n = q.g(5) & 4294967295
    x = readz(q, a, n).lower()
    y = readz(q, b, n).lower()
    q.setg(3, 0 if x == y else 1 if x > y else 4294967295)

def strlen(q):
    q.setg(3, len(readz(q, q.g(3) & 4294967295)))

def vecfloat(q):
    dst = q.g(3) & 4294967295
    src = q.g(4) & 4294967295
    q.mem.wr(dst, q.mem.rd(src, 4) * 4)

def sincos(q):
    angle = np.float32(q.f(1))

    def calc(phase):
        cycles = V.fma(angle, np.float32(0.15915493667125702), np.float32(phase))
        frac = np.float32(cycles - np.float32(math.floor(cycles)))
        fold = min(frac, np.float32(1 - frac))
        t = np.float32(fold - np.float32(0.25))
        t2 = np.float32(t * t)
        inner = V.fma(t2, np.float32(-71.17897033691406), np.float32(40.897369384765625))
        poly = V.fma(t2, inner, np.float32(-6.278042793273926))
        return np.float32(poly * t)
    q.mem.wr(q.g(4) & 4294967295, struct.pack('>f', calc(-0.25)))
    q.mem.wr(q.g(5) & 4294967295, struct.pack('>f', calc(0.0)))

def draw(q):
    p = q.g(3) & 4294967295
    index = struct.unpack('>I', q.mem.rd(p + 40, 4))[0]
    old = struct.unpack('>Q', q.mem.rd(p + 32, 8))[0]
    sample = struct.unpack('>f', q.mem.rd(p + 4 * index, 4))[0]
    new = old * 6364136223846793005 + 1 & 18446744073709551615
    bits = 1065353216 | old >> 32 >> 9
    q.mem.wr(p + 32, struct.pack('>Q', new))
    q.mem.wr(p + 4 * index, struct.pack('>I', bits))
    q.mem.wr(p + 40, struct.pack('>I', index + 1 & 7))
    q.setf(1, float(np.float32(sample - 1.0)))

def randvec(q):
    p = q.g(4) & 4294967295
    dst = q.g(3) & 4294967295
    cursor = struct.unpack('>I', q.mem.rd(p + 40, 4))[0]
    slot = cursor + 3 & 4
    sample = struct.unpack('>f', q.mem.rd(p + 4 * slot, 4))[0]
    old = struct.unpack('>Q', q.mem.rd(p + 32, 8))[0]
    new = old * 6364136223846793005 + 1 & 18446744073709551615
    bits = 1065353216 | old >> 32 >> 9
    q.mem.wr(p + 32, struct.pack('>Q', new))
    q.mem.wr(p + 4 * slot, struct.pack('>I', bits))
    q.mem.wr(p + 40, struct.pack('>I', slot + 1))
    q.mem.wr(dst, struct.pack('>4f', *[float(np.float32(sample - 1.0))] * 4))

def hashstr(q):
    q.setg(3, 305419896)

def start(q):
    h = 65537
    q.mem.wr(MOD + 24176 + 112, struct.pack('>I', h))
    q.setg(3, h)

def get(q):
    h = q.g(4) & 4294967295
    ptr = MOD + 24176 + (h & 127) * 112
    current = struct.unpack('>I', q.mem.rd(ptr, 4))[0]
    q.setg(3, ptr if current == h else 0)
errors = []
for i, text in enumerate(['x=1\ny=2\nz=3\nangle=0\nname=glass', 'x=4\ny=1\nz=2\nangle=90\nname=glass', 'x=-7\ny=3\nz=8\nangle=180\nname=glass', 'x=0\ny=0\nz=0\nname=random-angle']):
    outputs = []
    for native in (False, True):
        q = coff.cpu() if native else cpu()
        for addr, values in [(2197469392, [6.28318548] * 4), (2198316672, [0.15915493667125702] * 4), (2198316432, [-0.25, 0, -0.25, 0]), (2198316384, [-0.25] * 4), (2198315968, [1.0] * 4), (2198315952, [40.897369384765625] * 4), (2198316656, [-71.17897033691406] * 4), (2198316528, [-6.278042793273926] * 4)]:
            q.mem.add(addr, 16, struct.pack('>4f', *values))
        q.mem.wr(TEXT, text.encode() + b'\x00')
        q.mem.wr(DATA, struct.pack('>I', TEXT))
        q.mem.wr(MOD + 185276, struct.pack('>I', DATA))
        q.mem.wr(MOD + 193828, struct.pack('>10I', *[4294967295] * 10))
        q.mem.wr(MOD + 181184, struct.pack('>8fQ I', *[1.25] * 8, 1311768467463790320, 0))
        q.setg(3, MOD)
        q.setvf(1, [1.0, 2.0, 3.0, 0.0])
        if native:
            for addr, name in coff.unresolved.items():
                if name.startswith('??0VecFloat@'):
                    q.hooks[addr] = vecfloat
                elif 'strnicmp' in name:
                    q.hooks[addr] = nicmp
                elif 'strlen' in name:
                    q.hooks[addr] = strlen
                elif '?HashString@' in name:
                    q.hooks[addr] = hashstr
                elif '?StartLionEffect@' in name:
                    q.hooks[addr] = start
                elif '?GetLionEffect@' in name:
                    q.hooks[addr] = get
                elif '?RandomFloat@' in name:
                    q.hooks[addr] = draw
                elif '?RandomVecFloat@' in name:
                    q.hooks[addr] = randvec
                elif '?SinCosCycles@' in name:
                    q.hooks[addr] = sincos
                elif name.startswith(('__savefpr_', '__restfpr_', '__savevmx_', '__restvmx_')):
                    q.hooks[addr] = lambda c: None
        else:
            q.hooks[2193669440] = nicmp
            q.hooks[2183622648] = hashstr
            q.hooks[2183700304] = start
        q.run(coff.entry if native else 2183731944)
        outputs.append((q.mem.rd(MOD + 193828, 40).hex(), q.mem.rd(MOD + 24176 + 112, 112).hex(), q.mem.rd(MOD + 181184, 44).hex()))
    if outputs[0] != outputs[1]:
        errors.append({'case': i, 'original': outputs[0], 'compiled': outputs[1]})
r = {'cases': 4, 'mismatches': len(errors), 'examples': errors, 'object_sha256': hashlib.sha256(obj.read_bytes()).hexdigest(), 'scope': 'actual compiled JunkyardVfxStart vs ARTIST 82291AE8, three explicit-angle and one randomized-angle in-range locator records; captured handle, full LionEffect slot and RNG; hash/start/get/trig/random controlled hooks; no hardware claim'}
_r5_output(Path('D:/Reverse/BP-X360-Experimental-Artifacts/effects-native-terra/junkyard-start-spawn-comparison.json')).write_text(json.dumps(r, indent=2))
print(json.dumps(r, indent=2))
assert not errors