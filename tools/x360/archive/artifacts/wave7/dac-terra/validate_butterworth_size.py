import sys,json,random,struct,hashlib
from pathlib import Path
root=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(root/'scratch/x360_sdk'));import native_atomic_fixture as F
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave7/dac-terra');d=out/'butterworth-build';q=d/'Butterworth.obj';errors=[];rng=random.Random(0xdf88);selfaddr=F.M.S.SELF
for name,orig in [('HighPassButterworth',0x82b9df88),('LowPassButterworth',0x82b9e4b0)]:
 p=d/(name+'.obj');prefix='?GetSize@'+name+'@';b=F.Bundle([F.Coff(p,prefix),F.Coff(q,'?GetSize@Butterworth@')]);b.select(prefix)
 for i in range(128):
  img=bytearray(rng.randbytes(0x100));img[8]=i%9;both=[]
  for compiled in (False,True):
   c=F.cpu(b);c.mem.wr(selfaddr,img);c.setg(3,selfaddr);c.run(b.entry if compiled else orig);both.append((c.mem.rd(selfaddr,0x100),c.g(3)&0xffffffff))
  if both[0]!=both[1]:errors.append({'name':name,'case':i,'returns':[both[0][1],both[1][1]]})
r={'cases':256,'errors':errors,'limits':'Actual filter config GetSize→actual Butterworth GetSize, outputChannels 0–8. No heap/hardware claim.'};(out/'butterworth-size-comparison.json').write_text(json.dumps(r,indent=2)+'\n');print(len(errors),errors[:2]);assert not errors
