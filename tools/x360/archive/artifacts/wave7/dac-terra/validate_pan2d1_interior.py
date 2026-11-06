import sys,json,random,struct,hashlib,math
from pathlib import Path
root=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(root/'scratch/x360_sdk'));import native_atomic_fixture as F
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave7/dac-terra');p=out/'pan2d1-build/Pan2D1.obj';b=F.Bundle([F.Coff(p,'?ComputeInteriorTerm@Pan2D1@')]);b.select('?ComputeInteriorTerm@Pan2D1@');errors=[];rng=random.Random(0x8db8);selfaddr=F.M.S.SELF
for i in range(256):
 img=bytearray(rng.randbytes(0x300));struct.pack_into('>I',img,0xd0,5 if i&1 else 4);struct.pack_into('>f',img,0x48,(1+i%7)/8.0)
 for j,angle in enumerate([0.6,0.,-0.6,2.3,-2.3]):struct.pack_into('>2f',img,0x13c+j*8,math.cos(angle),math.sin(angle))
 index=(i//2)%5;x=((i%17)-8)/12.0;y=(((i*7)%17)-8)/12.0;struct.pack_into('>4f',img,0x164+16*index,x,y,(x*x+y*y)*0.5,0.0);both=[]
 for compiled in (False,True):
  c=F.cpu(b);c.mem.wr(selfaddr,img);c.setg(3,selfaddr);c.setg(4,index);c.run(b.entry if compiled else 0x82b98db8);both.append((c.mem.rd(selfaddr,0x300),c.g(3)&0xffffffff))
 if both[0]!=both[1]:errors.append({'case':i,'index':index,'diff':[hex(j) for j,(a,z) in enumerate(zip(both[0][0],both[1][0])) if a!=z][:30], 'returns':[both[0][1],both[1][1]]})
r={'cases':256,'errors':errors,'sha256':hashlib.sha256(p.read_bytes()).hexdigest(),'limits':'Actual ComputeInteriorTerm, original vs compiled memory and return; finite interior points, 4 or 5 speakers; no hardware or special-float claim.'};(out/'pan2d1-interior-comparison.json').write_text(json.dumps(r,indent=2)+'\n');print(len(errors),errors[:3]);assert not errors
