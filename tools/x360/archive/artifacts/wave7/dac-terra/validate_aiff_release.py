import sys,json,random,struct,hashlib
from pathlib import Path
root=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(root/'scratch/x360_sdk'));import native_atomic_fixture as F
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave7/dac-terra');p=out/'aiff-build/AiffWriter.obj';b=F.Bundle([F.Coff(p,'?ReleaseEvent@AiffWriter@')]);b.select('?ReleaseEvent@AiffWriter@');errors=[];rng=random.Random(0x53f8);selfaddr=F.M.S.SELF;aux=F.M.S.AUX
for i in range(128):
 initial=bytearray(rng.randbytes(0x100));struct.pack_into('>I',initial,4,aux+0x1000);struct.pack_into('>I',initial,0x40,aux+0x4000 if i&1 else 0);both=[]
 for compiled in (False,True):
  c=F.cpu(b);c.mem.wr(selfaddr,initial);c.setg(3,selfaddr);trace=[]
  def stop(x):
   rec=x.g(3)&0xffffffff;trace.append(('stop',int.from_bytes(x.mem.rd(rec+4,4),'big')));x.setg(3,8)
  def free(x):trace.append(('free',x.g(3)&0xffffffff,x.g(4)&0xffffffff,x.g(5)&0xffffffff))
  c.hooks[0x82ba1ec0]=stop;c.hooks[0x82b6be48]=free;c.hooks[F.address(b,'?StopHandler@AiffWriter@')]=stop
  for at,n in b.unresolved.items():
   if n.startswith('?Free@System@'):c.hooks[at]=free
  c.run(b.entry if compiled else 0x82ba53f8);both.append((c.mem.rd(selfaddr,0x100),trace))
 if both[0]!=both[1]:errors.append({'case':i,'trace':[both[0][1],both[1][1]]})
r={'cases':128,'errors':errors,'sha256':hashlib.sha256(p.read_bytes()).hexdigest(),'limits':'Actual ReleaseEvent; StopHandler and System::Free controlled; no file I/O or allocator proof.'};(out/'aiff-release-comparison.json').write_text(json.dumps(r,indent=2)+'\n');print(len(errors),errors[:2]);assert not errors

