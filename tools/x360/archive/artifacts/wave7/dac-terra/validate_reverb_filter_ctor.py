import sys,json,random,hashlib
from pathlib import Path
root=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(root/'scratch/x360_sdk'));import native_atomic_fixture as F
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave7/dac-terra');p=out/'reverb-filter-ctor-build/ReverbFilters.obj';errors=[];rng=random.Random(0xc3d8);selfaddr=F.M.S.SELF
for name,orig in [('AllPassFilter',0x82b6c3d8),('CombFilter',0x82b6c6b0)]:
 prefix='??0'+name+'@';b=F.Bundle([F.Coff(p,prefix)]);b.select(prefix)
 for i in range(128):
  img=bytearray(rng.randbytes(0x100));both=[]
  for compiled in (False,True):
   c=F.cpu(b);c.mem.wr(selfaddr,img);c.setg(3,selfaddr);c.run(b.entry if compiled else orig);both.append((c.mem.rd(selfaddr,0x100),c.g(3)&0xffffffff))
  if both[0]!=both[1]:errors.append({'name':name,'case':i,'diff':[hex(j) for j,(a,z) in enumerate(zip(both[0][0],both[1][0])) if a!=z]})
r={'cases':256,'errors':errors,'sha256':hashlib.sha256(p.read_bytes()).hexdigest(),'limits':'Actual AllPassFilter and CombFilter constructors, randomized memory and return; no filter DSP/hardware claim.'};(out/'reverb-filter-constructor-comparison.json').write_text(json.dumps(r,indent=2)+'\n');print(len(errors),errors[:2]);assert not errors
