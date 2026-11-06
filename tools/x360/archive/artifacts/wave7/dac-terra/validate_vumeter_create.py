import sys,json,random,struct,hashlib
from pathlib import Path
root=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(root/'scratch/x360_sdk'));import native_atomic_fixture as F
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave7/dac-terra');d=out/'vumeter-native-build';p=d/'VuMeter.obj';q=d/'TimerHandle.obj';b=F.Bundle([F.Coff(p,'?CreateInstance@VuMeter@'),F.Coff(q,'??0TimerHandle@')]);b.select('?CreateInstance@VuMeter@');errors=[];rng=random.Random(0xea8);selfaddr=F.M.S.SELF;aux=F.M.S.AUX
for i in range(128):
 img=bytearray(rng.randbytes(0x300));struct.pack_into('>I',img,4,aux+0x1000);img[0x21]=1+i%6;both=[]
 for compiled in (False,True):
  c=F.cpu(b);c.mem.wr(selfaddr,img);c.setg(3,selfaddr);trace=[]
  def timer(x):
   expected=F.address(b,'?RwacTimerClient@VuMeter@') if compiled else 0x82b9d0c8;assert x.g(5)&0xffffffff==expected;trace.append((x.g(3)&0xffffffff,x.g(4)&0xffffffff,x.g(6)&0xffffffff,x.g(8)&0xffffffff,x.g(9)&0xffffffff,F.cstring(x,x.g(7)&0xffffffff)));x.setg(3,i&1)
  c.hooks[0x82b6eb88]=timer
  for at,n in b.unresolved.items():
   if n.startswith('?AddTimer@TimerManager@'):c.hooks[at]=timer
  c.run(b.entry if compiled else 0x82ba0ea8);obj=bytearray(c.mem.rd(selfaddr,0x300));assert F.cstring(c,struct.unpack_from('>I',obj,0x30)[0])=='Unknown';obj[:4]=bytes(4);obj[0x30:0x34]=bytes(4);both.append((obj,trace,c.g(3)&0xffffffff))
 if both[0]!=both[1]:errors.append({'case':i,'diff':[hex(j) for j,(a,z) in enumerate(zip(both[0][0],both[1][0])) if a!=z][:30],'trace':[both[0][1],both[1][1]]})
r={'cases':128,'errors':errors,'sha256':hashlib.sha256(p.read_bytes()).hexdigest(),'timer_sha256':hashlib.sha256(q.read_bytes()).hexdigest(),'limits':'Actual VuMeter CreateInstance and real TimerHandle ctor; AddTimer controlled with callback identity checked, both outcomes; vtable and equal Unknown normalized; no real timer/hardware claim.'};(out/'vumeter-create-comparison.json').write_text(json.dumps(r,indent=2)+'\n');print(len(errors),errors[:1]);assert not errors
