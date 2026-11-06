import sys,json,random,struct,hashlib
from pathlib import Path
root=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(root/'scratch/x360_sdk'));import native_atomic_fixture as F
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave7/dac-terra');d=out/'reverb-native-build';objs=[d/'ReverbModel1.obj',d/'ReverbFilters.obj',d/'TimerHandle.obj',d/'DelayLine.obj'];prefix='?ReleaseEvent@ReverbModel1@';b=F.Bundle([F.Coff(objs[0],prefix),F.Coff(objs[1],'??0AllPassFilter@'),F.Coff(objs[2],'??0TimerHandle@'),F.Coff(objs[3],'??0DelayLine@')]);b.select(prefix);errors=[];rng=random.Random(0xadb0);selfaddr=F.M.S.SELF;aux=F.M.S.AUX
for i in range(128):
 img=bytearray(rng.randbytes(0x600));img[0x439]=i%4;img[0x43a]=i&1;both=[]
 for compiled in (False,True):
  c=F.cpu(b);c.mem.add(0x83271900,0x100);c.mem.wr(selfaddr,img);F.put(c,0x83271928,aux+0x1000);c.setg(3,selfaddr);trace=[]
  def release(x):trace.append(('release',x.g(3)&0xffffffff))
  def timer(x):trace.append(('timer',x.g(3)&0xffffffff,x.g(4)&0xffffffff))
  c.hooks[0x82b6cc08]=release;c.hooks[F.address(b,'?Release@DelayLine@')]=release;c.hooks[0x82b6eb80]=timer
  for at,n in b.unresolved.items():
   if n=='off_83271928':F.put(c,at,aux+0x1000)
   if n.startswith('?RemoveTimer@System@'):c.hooks[at]=timer
  c.run(b.entry if compiled else 0x82b9adb0);both.append((c.mem.rd(selfaddr,0x600),trace))
 if both[0]!=both[1]:errors.append({'case':i,'trace':[both[0][1],both[1][1]]})
r={'cases':128,'errors':errors,'sha256':hashlib.sha256(objs[0].read_bytes()).hexdigest(),'limits':'Actual ReleaseEvent state and release/timer call order; DelayLine::Release and System::RemoveTimer controlled; no allocator, concurrency or hardware claim.'};(out/'reverb-release-comparison.json').write_text(json.dumps(r,indent=2)+'\n');print(len(errors),errors[:1]);assert not errors

