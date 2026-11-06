import sys,json,random,struct,hashlib
from pathlib import Path
root=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(root/'scratch/x360_sdk'));import native_atomic_fixture as F
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave7/dac-terra');p=out/'resample-build/Resample.obj';errors=[];rng=random.Random(0xa860);selfaddr=F.M.S.SELF
for name,orig in [('GetSize',0x82b9f3d8),('SetResampleIncrement',0x82b9a860)]:
 prefix='?'+name+'@Resample@';b=F.Bundle([F.Coff(p,prefix)]);b.select(prefix)
 for i in range(128):
  initial=bytearray(rng.randbytes(0x100));initial[8]=i%9;both=[]
  for compiled in (False,True):
   c=F.cpu(b);c.mem.wr(selfaddr,initial);c.setg(3,selfaddr);c.setf(1,(i%81)/16.0);c.run(b.entry if compiled else orig);both.append((c.mem.rd(selfaddr,0x100),c.g(3)&0xffffffff))
  if both[0]!=both[1]:errors.append({'name':name,'case':i,'diff':[hex(j) for j,(a,z) in enumerate(zip(both[0][0],both[1][0])) if a!=z][:32], 'returns':[both[0][1],both[1][1]]})
r={'cases':256,'errors':errors,'sha256':hashlib.sha256(p.read_bytes()).hexdigest(),'limits':'GetSize and finite nonnegative SetResampleIncrement cases, no special float/concurrency/hardware claim.'};(out/'resample-size-increment-comparison.json').write_text(json.dumps(r,indent=2)+'\n');print(len(errors),errors[:2]);assert not errors
