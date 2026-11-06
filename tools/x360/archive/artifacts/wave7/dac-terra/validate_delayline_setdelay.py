import sys,json,random,struct,hashlib
from pathlib import Path
root=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(root/'scratch/x360_sdk'));import native_atomic_fixture as F
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave7/dac-terra');p=out/'delay-native-build/DelayLine.obj';b=F.Bundle([F.Coff(p,'?SetDelay@DelayLine@')]);b.select('?SetDelay@DelayLine@');errors=[];rng=random.Random(0x7f38);selfaddr=F.M.S.SELF
for i in range(128):
 img=bytearray(rng.randbytes(0x100));old=rng.randrange(0,1000);value=old if i&1 else rng.randrange(1001,2000);struct.pack_into('>I',img,0x28,old);both=[]
 for compiled in (False,True):
  c=F.cpu(b);c.mem.wr(selfaddr,img);c.setg(3,selfaddr);c.setg(4,value);c.run(b.entry if compiled else 0x82b67f38);both.append(c.mem.rd(selfaddr,0x100))
 if both[0]!=both[1]:errors.append({'case':i})
r={'cases':128,'errors':errors,'sha256':hashlib.sha256(p.read_bytes()).hexdigest(),'limits':'Actual DelayLine SetDelay changed/unchanged inputs and full state; no ring/hardware claim.'};(out/'delayline-setdelay-comparison.json').write_text(json.dumps(r,indent=2)+'\n');print(len(errors),errors[:1]);assert not errors
