import sys,json,random,struct,hashlib
from pathlib import Path
root=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(root/'scratch/x360_sdk'));import native_atomic_fixture as F
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave7/dac-terra');d=out/'snd9-native-build';p=d/'Snd9Service.obj';q=d/'TimerHandle.obj';b=F.Bundle([F.Coff(p,'?CreateInstance@Snd9Service@'),F.Coff(q,'??0TimerHandle@')]);b.select('?CreateInstance@Snd9Service@');errors=[];rng=random.Random(0x130);selfaddr=F.M.S.SELF;aux=F.M.S.AUX
for i in range(128):
 img=bytearray(rng.randbytes(0x200));struct.pack_into('>I',img,4,aux+0x1000);both=[]
 for compiled in (False,True):
  c=F.cpu(b);c.mem.wr(selfaddr,img);c.setg(3,selfaddr);trace=[]
  def alloc(x):trace.append(('alloc',x.g(3)&0xffffffff,x.g(4)&0xffffffff,x.g(6)&0xffffffff,x.g(7)&0xffffffff));x.setg(3,aux+0x8000)
  def timer(x):
   expected=F.address(b,'?RwacTimerClient@Snd9Service@') if compiled else 0x82b9bc38;assert x.g(5)&0xffffffff==expected;trace.append(('timer',*[x.g(z)&0xffffffff for z in (3,4,6,8,9)],F.cstring(x,x.g(7)&0xffffffff)));x.setg(3,i&1)
  c.hooks[0x82b6be18]=alloc;c.hooks[0x82b6eb88]=timer
  for at,n in b.unresolved.items():
   if n.startswith('?Alloc@System@'):c.hooks[at]=alloc
   if n.startswith('?AddTimer@TimerManager@'):c.hooks[at]=timer
  c.run(b.entry if compiled else 0x82ba0130);obj=bytearray(c.mem.rd(selfaddr,0x200));assert F.cstring(c,struct.unpack_from('>I',obj,0x30)[0])=='Unknown';obj[:4]=bytes(4);obj[0x30:0x34]=bytes(4);both.append((obj,trace,c.g(3)&0xffffffff))
 if both[0]!=both[1]:errors.append({'case':i,'diff':[hex(j) for j,(a,z) in enumerate(zip(both[0][0],both[1][0])) if a!=z][:24],'trace':[both[0][1],both[1][1]]})
r={'cases':128,'errors':errors,'sha256':hashlib.sha256(p.read_bytes()).hexdigest(),'timer_sha256':hashlib.sha256(q.read_bytes()).hexdigest(),'limits':'Actual Snd9 CreateInstance with real TimerHandle; controlled allocation success and AddTimer outcomes; callback identity verified, vtable/Unknown pointer normalized. No actual timer, allocator or hardware claim.'};(out/'snd9-create-success-comparison.json').write_text(json.dumps(r,indent=2)+'\n');print(len(errors),errors[:1]);assert not errors
