import sys,json,random,struct,hashlib
from pathlib import Path
root=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(root/'scratch/x360_sdk'));import native_atomic_fixture as F
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave7/dac-terra');p=out/'timer-ctor-build/AiffWriter.obj';q=out/'timer-ctor-build/TimerHandle.obj';b=F.Bundle([F.Coff(p,'?CreateInstance@AiffWriter@'),F.Coff(q,'??0TimerHandle@')]);b.select('?CreateInstance@AiffWriter@');errors=[];rng=random.Random(0xd488);selfaddr=F.M.S.SELF;aux=F.M.S.AUX
for i in range(128):
 initial=bytearray(rng.randbytes(0x100));initial[0x21]=i%7;struct.pack_into('>I',initial,4,aux+0x1000);both=[]
 for compiled in (False,True):
  c=F.cpu(b);c.mem.wr(selfaddr,initial);c.setg(3,selfaddr);trace=[]
  def alloc(x):
   trace.append((x.g(3)&0xffffffff,x.g(4)&0xffffffff,x.g(6)&0xffffffff,x.g(7)&0xffffffff));x.setg(3,aux+0x8000 if i&1 else 0)
  c.hooks[0x82b6be18]=alloc
  for at,n in b.unresolved.items():
   if n.startswith('?Alloc@System@'):c.hooks[at]=alloc
  c.run(b.entry if compiled else 0x82b9d488);obj=bytearray(c.mem.rd(selfaddr,0x100));addr=struct.unpack_from('>I',obj,0x30)[0];assert F.cstring(c,addr)=='Unknown';obj[:4]=bytes(4);obj[0x30:0x34]=bytes(4);both.append((obj,c.g(3)&0xffffffff,trace))
 if both[0]!=both[1]:errors.append({'case':i,'diff':[hex(j) for j,(a,z) in enumerate(zip(both[0][0],both[1][0])) if a!=z][:20],'trace':[both[0][2],both[1][2]]})
r={'cases':128,'errors':errors,'sha256':hashlib.sha256(p.read_bytes()).hexdigest(),'timer_sha256':hashlib.sha256(q.read_bytes()).hexdigest(),'limits':'Actual AiffWriter+TimerHandle constructors, System::Alloc controlled; vtable and equal Unknown string address normalized. No allocator/hardware claim.'};(out/'timer-aiff-create-comparison.json').write_text(json.dumps(r,indent=2)+'\n');print(len(errors),errors[:2]);assert not errors
