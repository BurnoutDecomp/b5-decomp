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
obj=Path('D:/Reverse/BP-X360-Experimental-Artifacts/racecar_vecfloat/fpu_curves_plain/probe.obj');coff=Coff(obj,'?Evaluate@SmoothStep@Curves@BrnEffects@@SAMABV');rng=random.Random(3611);errors=[]
import itertools, math, warnings
warnings.filterwarnings("ignore")
paramsets=[(0.,0.,0.),(0.,0.,1.),(0.,1.,1.),(-1.,0.,1.),(-float("inf"),0.,float("inf"))]
scales=[(0.,1.),(-0.,0.),(-5.,7.),(float("inf"),1.),(float("nan"),1.)]
values=[-float("inf"),-1.,-0.,0.,0.5,1.,float("inf"),float("nan")]
cases=list(itertools.product(paramsets,scales,values))
for i,(params,sc,v) in enumerate(cases):
 a,b,c=params;outputs=[]
 for native in (False,True):
  q=coff.cpu() if native else cpu();q.mem.wr(PARAM,struct.pack('>3f',a,b,c));q.mem.wr(SCALE,struct.pack('>2f',*sc));q.setg(3,PARAM);q.setg(4,SCALE);q.setf(1,v);q.run(coff.entry if native else 0x8227d2f0);outputs.append(struct.pack('>f',q.f(1)))
 if outputs[0]!=outputs[1]:errors.append({'case':i,'original':outputs[0].hex(),'compiled':outputs[1].hex()})
r={'cases':len(cases),'mismatches':len(errors),'examples':errors[:8],'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'actual original-vs-compiled FPU curve using zero spans, signed zero, infinities and NaNs; emulator models FP edge behavior; no hardware claim'};Path('D:/Reverse/BP-X360-Experimental-Artifacts/racecar_vecfloat/fpu-curves-special-comparison.json').write_text(json.dumps(r,indent=2));print(json.dumps(r,indent=2));assert not errors
