from pathlib import Path
import ast,re,json,struct,random,hashlib,sys
sys.path.insert(0,'D:/Reverse/BP-Decomp_Workflow/scratch/CRASHPARITY_0922/fxcrashvfx_vmxemu');import emu64 as E,vmxemu as V
p=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/scratch/x360_sdk/validate_render_states.py');t=ast.parse(p.read_text());exec(compile(ast.Module(body=[n for n in t.body if isinstance(n,ast.ClassDef) and n.name=='Coff'],type_ignores=[]),'coff','exec'),globals())
DATA=0x40000000; STACK=0x44000000
class CPU(E.CPU64):
 def exec_vector(self,pc,w,mn):
  native = coff.entry
  if (w >> 26)==4 and (w & 0x7ff)==644:
   d=(w>>21)&31;a=(w>>16)&31;b=(w>>11)&31;self.vr[d]=tuple(self.vr[a][i] >> (self.vr[b][i]&31) for i in range(4));return True
  if pc in (0x822cfc58,native+0x34):
   self.setvf(11 if pc==0x822cfc58 else 7,[0.,0.,0.,1.]);return True
  if pc in (0x822cfcdc,native+0x8c):
   dst,src=(28,5) if pc==0x822cfcdc else (3,12)
   import numpy as np
   with np.errstate(all='ignore'): self.setvf(dst,[np.float32(np.log2(x)) for x in self.vf(src)])
   return True
  if pc in (0x822cfd40,native+0x100):
   dst,src=(30,0) if pc==0x822cfd40 else (23,13)
   import numpy as np
   self.setvf(dst,[np.trunc(x) if np.isfinite(x) else x for x in self.vf(src)]);return True
  if pc in (0x822cfd98,native+0x104):
   dst,src=(31,0) if pc==0x822cfd98 else (22,13)
   import numpy as np
   self.vr[dst]=tuple((max(-2147483648,min(2147483647,int(x))) & 0xffffffff) if np.isfinite(x) else (0x80000000 if x<0 else 0x7fffffff) for x in self.vf(src));return True
  if pc in (0x822cfd6c,native+0x114):
   dst,src=(2,12) if pc==0x822cfd6c else (30,11)
   import numpy as np
   with np.errstate(all='ignore'): self.setvf(dst,[np.float32(np.exp2(x)) for x in self.vf(src)])
   return True
  return super().exec_vector(pc,w,mn)
 def step(self,pc,w,nxt,depth):
  if pc==0x822cfe1c:raise E.Return()
  return super().step(pc,w,nxt,depth)
def cpu():
 m=V.Mem();m.add(DATA,0x1000);m.add(STACK,0x20000);c=CPU(m);c.setg(1,STACK+0x10000);return c
class M:pass
M.new_cpu=staticmethod(cpu)
obj=Path('D:/Reverse/BP-X360-Experimental-Artifacts/racecar_vecfloat/fog_vmx_probe/probe.obj');coff=Coff(obj,'FogProbe');rng=random.Random(3609);errors=[]
for i in range(256):
 dist=[rng.uniform(0,100)]*4;sc=[rng.uniform(-2,2),rng.uniform(-2,2),rng.uniform(-2,4),rng.uniform(0,2)];col=[rng.uniform(-2,2) for _ in range(4)];o=[]
 for native in (False,True):
  c=coff.cpu() if native else cpu();c.mem.wr(DATA,struct.pack('>4f',*dist));c.mem.wr(DATA+16,struct.pack('>4f',*sc));c.mem.wr(DATA+32,struct.pack('>4f',*col));
  if native:
   for r,v in [(3,DATA),(4,DATA+16),(5,DATA+32),(6,DATA+48)]:c.setg(r,v)
   c.run(coff.entry);o.append(c.mem.rd(DATA+48,16))
  else:
   c.mem.wr(STACK+0x10000+0x5c0,struct.pack('>4f',*sc));c.setvf(122,dist);c.setvf(120,col);c.run(0x822cfc50);o.append(struct.pack('>4I',*c.vr[1]))
 if o[0]!=o[1]:errors.append({'case':i,'original':o[0].hex(),'candidate':o[1].hex()})
r={'cases':256,'mismatches':len(errors),'examples':errors[:10],'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'original vector fog instruction slice vs compiled equivalent fixture, finite randomized inputs; emulator estimates are software models, not physical hardware; no full product proof'};Path('D:/Reverse/BP-X360-Experimental-Artifacts/racecar_vecfloat/fog-vmx-comparison.json').write_text(json.dumps(r,indent=2));print(json.dumps(r,indent=2));assert not errors
