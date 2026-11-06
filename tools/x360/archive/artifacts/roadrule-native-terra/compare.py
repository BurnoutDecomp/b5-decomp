import json,struct,sys,random,hashlib
import numpy as np
from pathlib import Path
sys.path.insert(0,r'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/scratch/x360_sdk');import native_atomic_fixture as F
class RoadCPU(F.CPU):
 def exec_vector(self,a,w,mn):
  if w>>26==6 and w&0x7f0==0x670:
   d=((w>>21)&31)|(((w>>2)&3)<<5);y=((w>>11)&31)|((w&3)<<5)
   self.setvf(d,[np.float32(1.0/np.sqrt(np.float64(x))) for x in self.vf(y)]);return True
  if w>>26==5 and w&0x3d0==0x250:
   d=((w>>21)&31)|(((w>>2)&3)<<5);x=((w>>16)&31)|(((w>>5)&1)<<5)|(((w>>10)&1)<<6);y=((w>>11)&31)|((w&3)<<5)
   self.vr[d]=tuple(self.vr[x][i]&~self.vr[y][i]&0xffffffff for i in range(4));return True
  return super().exec_vector(a,w,mn)
p=Path(r'D:/Reverse/BP-X360-Experimental-Artifacts/roadrule-native-terra');row=json.loads((p/'whole2/compile-results.json').read_text())['results'][0];ob=F.Coff(Path(row['object']),'?UpdateRoadSignDistance@');b=F.Bundle([ob]);b.select('?UpdateRoadSignDistance@'); key=next(v for k,v in b.definitions.items() if k.startswith('?GetSignOffsetSizeAdjustment@'));offaddr=b.bases[(key[0],key[2])]+key[1];base=F.M.S.SELF; sign=F.M.S.AUX
bits=lambda f:struct.unpack('>I',struct.pack('>f',f))[0]
def go(original,cam,road,side,adjust,offset,active):
 c=b.cpu(); c.__class__=RoadCPU; c.mem.add(0x82fb0000,0x10000);c.mem.add(0x84800000,0x1000)
 def store(a, vals):c.mem.wr(a,struct.pack('>4f',*vals))
 for a,val in [(0x82fb2d10,50.),(0x82fb29f0,100.),(0x82fb2c10,.01),(0x82fb2cf0,300.),(0x82fb2ad0,50.),(0x82fb2aa0,0.),(0x82fb2ab0,-160.)]:store(a,[val]*4)
 store(base+0x1d0+side*16,road);c.wr32(sign+0x14,1 if active else 0);c.setg(3,base);c.setg(4,side);c.setg(5,sign);c.setvf(1,cam);c.setvf(2,[adjust]*4);out=[]
 def adjustment(q):store(q.g(3),[offset]*4)
 def setpos(q):out.append(tuple(q.vr[1]))
 def ctor(q):val=q.mem.rd(q.g(4),4);q.mem.wr(q.g(3),val*4)
 for addr,name in b.unresolved.items():
  if name.startswith('??0VecFloat@'):c.hooks[addr]=ctor
  elif name.startswith('?SetPosition@'):c.hooks[addr]=setpos
  elif name.startswith('__savevmx_') or name.startswith('__restvmx_'):c.hooks[addr]=lambda q:None
 c.hooks[offaddr]=adjustment;c.hooks[0x824140f8]=adjustment;c.hooks[0x8241dfc0]=setpos
 c.run(0x8242aff8 if original else b.entry)
 return out
rng=random.Random(3608242);bad=[]
for i in range(256):
 cam=[rng.uniform(-500,500) for _ in range(4)];road=[rng.uniform(-500,500) for _ in range(4)];side=i%2;adjust=(-1.,1.)[side];offset=rng.choice([0.,4.,-18.,6.5]);active=(i%5)!=0
 a=go(True,cam,road,side,adjust,offset,active);z=go(False,cam,road,side,adjust,offset,active)
 if a!=z:bad.append({'case':i,'original':a,'native':z})
for j,delta in enumerate([0.0,0.0001,0.000345266983,0.0004,49.0,50.0,150.0,250.0]):
 cam=[delta,99.,0.,0.];road=[0.,-5.,0.,0.];a=go(True,cam,road,0,1.,4.,True);z=go(False,cam,road,0,1.,4.,True)
 if a!=z:bad.append({'case':'boundary'+str(j),'original':a,'native':z})
print('cases',264,'mismatches',len(bad),bad[:3]);report={'cases':264,'mismatches':bad,'object_sha256':hashlib.sha256(Path(row['object']).read_bytes()).hexdigest(),'scope':'full original and actual compiled method, same finite input, initialized data, controlled GetSignOffsetSizeAdjustment/SetPosition; all output lanes; modeled estimate, not hardware or complete runtime'};(p/'distance-comparison.json').write_text(json.dumps(report,indent=2)+'\n');assert not bad
