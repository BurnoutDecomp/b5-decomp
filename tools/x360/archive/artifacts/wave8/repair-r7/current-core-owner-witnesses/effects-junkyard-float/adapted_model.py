from pathlib import Path
import ast, json, struct, random, hashlib, sys, re
sys.path.insert(0, 'D:/Reverse/BP-Decomp_Workflow/scratch/CRASHPARITY_0922/fxcrashvfx_vmxemu')
import emu64 as E, vmxemu as V
p = Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/scratch/x360_sdk/validate_render_states.py')
t = ast.parse(p.read_text())
exec(compile(ast.Module(body=[n for n in t.body if isinstance(n, ast.ClassDef) and n.name == 'Coff'], type_ignores=[]), 'coff', 'exec'), globals())
TEXT = 1073741824
STACK = 1140850688

class CPU(E.CPU64):
    pass

def cpu():
    m = V.Mem()
    m.add(TEXT, 4096)
    m.add(STACK, 131072)
    c = CPU(m)
    c.setg(1, STACK + 65536)
    return c

class M:
    pass
M.new_cpu = staticmethod(cpu)
obj = _r5_object(Path('D:/Reverse/BP-X360-Experimental-Artifacts/effects-native-terra/gate-final3/EffectsModule-87aee4b9.obj'))
coff = Coff(obj, '?JunkyardVfxStringToFloat@EffectsModule@BrnEffects@@')
rng = random.Random(3607)
inputs = ['0', '-0', '1', '-1', '.5', '-.5', '0.000001', '123456.789', '-99.123456', '1.'] + ['%s%d.%06d' % ('-' if rng.randrange(2) else '', rng.randrange(100000), rng.randrange(1000000)) for _ in range(118)]
errors = []
for value in inputs:
    outputs = []
    for native in (False, True):
        q = coff.cpu() if native else cpu()
        q.mem.wr(TEXT, value.encode() + b'\x00')
        q.setg(3, TEXT)
        if native:
            for addr, name in coff.unresolved.items():
                if name.startswith('__savegprlr_') or name.startswith('__restgprlr_'):
                    q.hooks[addr] = lambda c: None
        q.run(coff.entry if native else 2183632632)
        outputs.append(struct.pack('>f', q.f(1)))
    if outputs[0] != outputs[1]:
        errors.append({'input': value, 'original': outputs[0].hex(), 'compiled': outputs[1].hex()})
r = {'cases': len(inputs), 'mismatches': len(errors), 'examples': errors[:8], 'object_sha256': hashlib.sha256(obj.read_bytes()).hexdigest(), 'scope': 'actual compiled JunkyardVfxStringToFloat versus ARTIST 822796F8, valid signed decimal strings, exact f32 return; excludes malformed input assert, caller and hardware'}
_r5_output(Path('D:/Reverse/BP-X360-Experimental-Artifacts/effects-native-terra/junkyard-float-comparison.json')).write_text(json.dumps(r, indent=2))
print(json.dumps(r, indent=2))
assert not errors