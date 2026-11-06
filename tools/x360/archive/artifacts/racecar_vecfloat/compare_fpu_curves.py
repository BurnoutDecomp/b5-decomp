from pathlib import Path
import ast,re,json,struct,random,hashlib,sys
sys.path.insert(0,'D:/Reverse/BP-Decomp_Workflow/scratch/CRASHPARITY_0922/fxcrashvfx_vmxemu');import emu64 as E,vmxemu as V
p=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/scratch/x360_sdk/validate_render_states.py');t=ast.parse(p.read_text());exec(compile(ast.Module(body=[n for n in t.body if isinstance(n,ast.ClassDef) and n.name=='Coff'],type_ignores=[]),'coff','exec'),globals())
PARAM=0x40000000;SCALE=0x41000000;STACK=0x44000000
class CPU(E.CPU64):pass
def cpu():
 m=V.Mem();m.add(PARAM,0x100);m.add(SCALE,0x100);m.add(STACK,0x20000);c=CPU(m);c.setg(1,STACK+0x10000);return c
class M:pass
M.new_cpu=staticmethod(cpu)
obj=Path('D:/Reverse/BP-X360-Experimental-Artifacts/racecar_vecfloat/fpu_curves/probe.obj');coff=Coff(obj,'?Evaluate@SmoothStep@Curves@BrnEffects@@SAMABV');rng=random.Random(3611);errors=[]
for i in range(1024):
 a=rng.uniform(-1e4,1e4);b=a+rng.uniform(.01,100);c=b+rng.uniform(.01,100);sc=[rng.uniform(-20,20),rng.uniform(-20,20)];v=[a,b,c,rng.uniform(a-100,c+100),a,b,c][i%7] if False else rng.choice([rng.uniform(a-100,c+100),a,b,c]);outputs=[]
 for native in (False,True):
  q=coff.cpu() if native else cpu();q.mem.wr(PARAM,struct.pack('>3f',a,b,c));q.mem.wr(SCALE,struct.pack('>2f',*sc));q.setg(3,PARAM);q.setg(4,SCALE);q.setf(1,v);q.run(coff.entry if native else 0x8227d2f0);outputs.append(struct.pack('>f',q.f(1)))
 if outputs[0]!=outputs[1]:errors.append({'case':i,'original':outputs[0].hex(),'compiled':outputs[1].hex()})
r={'cases':1024,'mismatches':len(errors),'examples':errors[:8],'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'actual compiled static FPU3/FPU2 Evaluate vs ARTIST 8227D2F0, finite ordered thresholds and inclusive boundary inputs, exact f32 return; no zero-span, NaN, caller or hardware claim'};Path('D:/Reverse/BP-X360-Experimental-Artifacts/racecar_vecfloat/fpu-curves-comparison.json').write_text(json.dumps(r,indent=2));print(json.dumps(r,indent=2));assert not errors
