import sys,json,random,hashlib
from pathlib import Path
root=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(root/'scratch/x360_sdk'));import native_atomic_fixture as F
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave7/dac-terra');p=out/'vumeter-native-build/VuMeter.obj';b=F.Bundle([F.Coff(p,'?UpdateAttributes@VuMeter@')]);b.select('?UpdateAttributes@VuMeter@');errors=[];rng=random.Random(0xd158);selfaddr=F.M.S.SELF
for i in range(128):
 img=bytearray(rng.randbytes(0x300));img[0x21]=[1,2,4,6,3][i%5];both=[]
 for compiled in (False,True):
  c=F.cpu(b);c.mem.wr(selfaddr,img);c.setg(3,selfaddr);
  for at,n in b.unresolved.items():
   if n.startswith(('__savefpr_', '__restfpr_')):c.hooks[at]=lambda x:None
  c.run(b.entry if compiled else 0x82b9d158);both.append(c.mem.rd(selfaddr,0x300))
 if both[0]!=both[1]:errors.append({'case':i,'diff':[hex(j) for j,(a,z) in enumerate(zip(both[0],both[1])) if a!=z][:20]})
r={'cases':128,'errors':errors,'sha256':hashlib.sha256(p.read_bytes()).hexdigest(),'limits':'Actual UpdateAttributes full state with random raw values, channels 1/2/4/6/other; FPR save/restore controlled; no DSP/timer/hardware claim.'};(out/'vumeter-attributes-comparison.json').write_text(json.dumps(r,indent=2)+'\n');print(len(errors),errors[:2]);assert not errors

