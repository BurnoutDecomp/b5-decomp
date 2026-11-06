import sys,json,random,struct,hashlib
from pathlib import Path
root=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(root/'scratch/x360_sdk'));import native_atomic_fixture as F
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave7/dac-terra');d=out/'snd9-native-build';p=d/'Snd9Service.obj';q=d/'TimerHandle.obj';b=F.Bundle([F.Coff(p,'?ReleaseEvent@Snd9Service@'),F.Coff(q,'??0TimerHandle@')]);b.select('?ReleaseEvent@Snd9Service@');errors=[];rng=random.Random(0xbb38);selfaddr=F.M.S.SELF;aux=F.M.S.AUX
for i in range(128):
 img=bytearray(rng.randbytes(0x200));struct.pack_into('>I',img,4,aux+0x1000);struct.pack_into('>I',img,0x3c,aux+0x8000 if i&1 else 0);img[0x68]=(i//2)%3;both=[]
 for compiled in (False,True):
  c=F.cpu(b);c.mem.add(0x83271900,0x100);c.mem.wr(selfaddr,img);F.put(c,0x83271928,aux+0x2000);c.setg(3,selfaddr);trace=[]
  def timer(x):trace.append(('timer',x.g(3)&0xffffffff,x.g(4)&0xffffffff))
  def restore(x):trace.append(('restore',));x.setg(3,0x1234)
  def free(x):trace.append(('free',x.g(3)&0xffffffff,x.g(4)&0xffffffff,x.g(5)&0xffffffff))
  c.hooks[0x82b6eb80]=timer;c.hooks[0x82b7e4f8]=restore;c.hooks[0x82b6be48]=free
  for at,n in b.unresolved.items():
   if n=='off_83271928':F.put(c,at,aux+0x2000)
   if n.startswith('?RemoveTimer@System@'):c.hooks[at]=timer
   if n=='SNDSYS_restore':c.hooks[at]=restore
   if n.startswith('?Free@System@'):c.hooks[at]=free
  c.run(b.entry if compiled else 0x82b9bb38);both.append((c.mem.rd(selfaddr,0x200),trace))
 if both[0]!=both[1]:errors.append({'case':i,'trace':[both[0][1],both[1][1]]})
r={'cases':128,'errors':errors,'sha256':hashlib.sha256(p.read_bytes()).hexdigest(),'limits':'Actual ReleaseEvent state/call order, timer removal, driver restore and allocator free controlled; no real driver/heap/concurrency/hardware claim.'};(out/'snd9-release-comparison.json').write_text(json.dumps(r,indent=2)+'\n');print(len(errors),errors[:1]);assert not errors
