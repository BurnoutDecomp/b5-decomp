from pathlib import Path
import ast, json, struct, hashlib, sys, re
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
errors = []
for i, text in enumerate(['', '  \r\n', '// line\n', 'name=unmatched', 'x=120\ny=0\nz=0\nangle=0\nname=foo\n', '// entry\nx=-180\ny=4\nz=1\nangle=45\nname=glass\n', 'x=0\ny=0\nz=200\nangle=90\nname=far']):
    outputs = []
    for native in (False, True):
        q = coff.cpu() if native else cpu()
        q.mem.wr(TEXT, text.encode() + b'\x00')
        q.mem.wr(DATA, struct.pack('>I', TEXT))
        q.mem.wr(MOD + 185276, struct.pack('>I', DATA))
        q.mem.wr(MOD + 193828, struct.pack('>10I', *[4294967295] * 10))
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
                elif name.startswith(('__savefpr_', '__restfpr_', '__savevmx_', '__restvmx_')):
                    q.hooks[addr] = lambda c: None
        else:
            q.hooks[2193669440] = nicmp
        q.run(coff.entry if native else 2183731944)
        outputs.append(q.mem.rd(MOD + 193828, 40).hex())
    if outputs[0] != outputs[1]:
        errors.append({'case': i, 'original': outputs[0], 'compiled': outputs[1]})
r = {'cases': 7, 'mismatches': len(errors), 'examples': errors, 'object_sha256': hashlib.sha256(obj.read_bytes()).hexdigest(), 'scope': 'actual compiled JunkyardVfxStart vs ARTIST 82291AE8, empty/comment/unmatched and three out-of-range explicit-angle locator lists; return/no handle changes; strnicmp hook, no spawn, transform, RNG, error or hardware claim'}
_r5_output(Path('D:/Reverse/BP-X360-Experimental-Artifacts/effects-native-terra/junkyard-start-empty-comparison.json')).write_text(json.dumps(r, indent=2))
print(json.dumps(r, indent=2))
assert not errors