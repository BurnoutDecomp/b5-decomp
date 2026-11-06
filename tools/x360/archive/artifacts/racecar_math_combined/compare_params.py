from pathlib import Path
import ast,re,json,struct,random,hashlib,sys
sys.path.insert(0,'D:/Reverse/BP-Decomp_Workflow/scratch/CRASHPARITY_0922/fxcrashvfx_vmxemu');import emu64 as E,vmxemu as V
p=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/scratch/x360_sdk/validate_render_states.py');t=ast.parse(p.read_text());exec(compile(ast.Module(body=[n for n in t.body if isinstance(n,ast.ClassDef) and n.name=='Coff'],type_ignores=[]),'coff','exec'),globals())
SELF=0x40000000; ARG=0x41000000; STACK=0x44000000
class CPU(E.CPU64): pass
def cpu():
 m=V.Mem();m.add(SELF,0x2000);m.add(ARG,0x1000);m.add(STACK,0x20000);c=CPU(m);c.setg(1,STACK+0x10000);return c
class M:pass
M.new_cpu=staticmethod(cpu)
obj=Path('D:/Reverse/BP-X360-Experimental-Artifacts/racecar_math_combined/params/probe.obj');specs=[('?GetCrackedGlassScaleFactorsN@RenderParams@',0x822b8410,'getter'),('?SetCrackedGlassScaleFactorsN@RenderParams@',0x822b83a0,'setter')];rng=random.Random(3605);errs=[];results=[]
for prefix,address,kind in specs:
 coff=Coff(obj,prefix)
 for i in range(128):
  data=bytearray(rng.randbytes(0x14a0));arg=bytearray(rng.randbytes(32));n=i%8
  for j in range(8):struct.pack_into('>2f',data,0x1458+8*j,rng.uniform(-100,100),rng.uniform(-100,100))
  if kind=='setter':struct.pack_into('>2f',arg,0,rng.uniform(-100,100),rng.uniform(-100,100))
  outputs=[]
  for compiled in (False,True):
   c=coff.cpu() if compiled else cpu();c.mem.wr(SELF,data);c.mem.wr(ARG,arg)
   if kind=='getter': c.setg(3,ARG);c.setg(4,SELF);c.setg(5,n)
   else:c.setg(3,SELF);c.setg(4,n);c.setg(5,ARG)
   c.run(coff.entry if compiled else address);outputs.append((c.g(3)&0xffffffff if kind=='getter' else None,c.mem.rd(SELF,0x14a0),c.mem.rd(ARG,32)))
  if outputs[0]!=outputs[1]:errs.append({'kind':kind,'case':i,'self_diffs':[j for j,(a,b) in enumerate(zip(outputs[0][1],outputs[1][1])) if a!=b][:10],'arg_diffs':[j for j,(a,b) in enumerate(zip(outputs[0][2],outputs[1][2])) if a!=b][:10]})
 results.append({'kind':kind,'cases':128,'symbol':coff.fn[0]})
res={'results':results,'total_mismatches':len(errs),'examples':errs[:5],'object':str(obj),'sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'actual compiled RenderParams functions vs ARTIST 822B8410/822B83A0; valid indices and finite scale values; entire 0x14a0 params + output sentinel checked; assertion failure paths excluded'};Path('D:/Reverse/BP-X360-Experimental-Artifacts/racecar_math_combined/glass-scale-comparison.json').write_text(json.dumps(res,indent=2));print(json.dumps(res,indent=2));assert not errs
