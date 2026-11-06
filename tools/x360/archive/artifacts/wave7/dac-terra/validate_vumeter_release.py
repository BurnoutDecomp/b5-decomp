import sys,json,random,struct,hashlib
from pathlib import Path
root=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(root/'scratch/x360_sdk'));import native_atomic_fixture as F
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave7/dac-terra');p=out/'vumeter-native-build/VuMeter.obj';b=F.Bundle([F.Coff(p,'?ReleaseEvent@VuMeter@')]);b.select('?ReleaseEvent@VuMeter@');errors=[];rng=random.Random(0xd0a8);selfaddr=F.M.S.SELF;aux=F.M.S.AUX
for i in range(128):
 img=bytearray(rng.randbytes(0x300));struct.pack_into('>I',img,4,aux+0x1000);img[0x14d]=i%3;both=[]
 for compiled in (False,True):
  c=F.cpu(b);c.mem.wr(selfaddr,img);c.setg(3,selfaddr);trace=[]
  def timer(x):trace.append((x.g(3)&0xffffffff,x.g(4)&0xffffffff))
  c.hooks[0x82b6eb80]=timer
  for at,n in b.unresolved.items():
   if n.startswith('?RemoveTimer@System@'):c.hooks[at]=timer
  c.run(b.entry if compiled else 0x82b9d0a8);both.append((c.mem.rd(selfaddr,0x300),trace))
 if both[0]!=both[1]:errors.append({'case':i,'trace':[both[0][1],both[1][1]]})
r={'cases':128,'errors':errors,'sha256':hashlib.sha256(p.read_bytes()).hexdigest(),'limits':'Actual ReleaseEvent state and call order; RemoveTimer controlled; no concurrency/hardware claim.'};(out/'vumeter-release-comparison.json').write_text(json.dumps(r,indent=2)+'\n');print(len(errors),errors[:1]);assert not errors
