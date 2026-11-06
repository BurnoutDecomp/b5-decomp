from pathlib import Path
import ast, json, struct, hashlib, sys, re
sys.path.insert(0, 'D:/Reverse/BP-Decomp_Workflow/scratch/CRASHPARITY_0922/fxcrashvfx_vmxemu')
import emu64 as E, vmxemu as V
p = Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/scratch/x360_sdk/validate_render_states.py')
t = ast.parse(p.read_text())
exec(compile(ast.Module(body=[n for n in t.body if isinstance(n, ast.ClassDef) and n.name == 'Coff'], type_ignores=[]), 'coff', 'exec'), globals())
TEXT = 1073741824
LABEL = 1090519040
OUT = 1107296256
CUR = 1124073472
STACK = 1140850688

class CPU(E.CPU64):
    pass

def cpu():
    m = V.Mem()
    for a, n in [(TEXT, 4096), (LABEL, 4096), (OUT, 4096), (CUR, 4096), (STACK, 131072)]:
        m.add(a, n)
    c = CPU(m)
    c.setg(1, STACK + 65536)
    return c

class M:
    pass
M.new_cpu = staticmethod(cpu)
obj = _r5_object(Path('D:/Reverse/BP-X360-Experimental-Artifacts/effects-native-terra/gate-final3/EffectsModule-87aee4b9.obj'))
coff = Coff(obj, '?JunkyardVfxReadLine@EffectsModule@BrnEffects@@')
cases = [('x', 'x=1'), ('x', ' \r\n x = -2.4 next=3'), ('angle', 'name=foo'), ('X', '// hello\r\nx=12.5\n'), ('name', 'name=glass.lef'), ('z', ''), ('x', '// no trailing newline')]

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
errors = []
for label, text in cases:
    outputs = []
    for native in (False, True):
        q = coff.cpu() if native else cpu()
        q.mem.wr(TEXT, text.encode() + b'\x00')
        q.mem.wr(LABEL, label.encode() + b'\x00')
        q.mem.wr(OUT, bytes([170]) * 256)
        q.mem.wr(CUR, struct.pack('>I', TEXT))
        q.setg(3, LABEL)
        q.setg(4, OUT)
        q.setg(5, 200)
        q.setg(6, CUR)
        if native:
            for addr, name in coff.unresolved.items():
                if 'strnicmp' in name:
                    q.hooks[addr] = nicmp
                elif 'strlen' in name:
                    q.hooks[addr] = strlen
                elif name.startswith('__savegprlr_') or name.startswith('__restgprlr_'):
                    q.hooks[addr] = lambda c: None
        else:
            q.hooks[2193669440] = nicmp
            q.hooks[2193662396] = lambda c: None
            q.hooks[2193663060] = lambda c: None
        q.run(coff.entry if native else 2183631856)
        outputs.append((q.g(3) & 4294967295, struct.unpack('>I', q.mem.rd(CUR, 4))[0] - TEXT, q.mem.rd(OUT, 32).hex()))
    if outputs[0] != outputs[1]:
        errors.append({'case': [label, text], 'original': outputs[0], 'compiled': outputs[1]})
r = {'cases': len(cases), 'mismatches': len(errors), 'examples': errors, 'object_sha256': hashlib.sha256(obj.read_bytes()).hexdigest(), 'scope': 'actual compiled JunkyardVfxReadLine vs ARTIST 822793F0; labels, whitespace, comments, EOF, mismatch and normal cursor/output; strnicmp hook; excludes asserts and hardware'}
_r5_output(Path('D:/Reverse/BP-X360-Experimental-Artifacts/effects-native-terra/junkyard-line-comparison.json')).write_text(json.dumps(r, indent=2))
print(json.dumps(r, indent=2))
assert not errors