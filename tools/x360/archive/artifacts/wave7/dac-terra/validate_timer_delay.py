import sys,json,random,struct,hashlib
from pathlib import Path
root=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(root/'scratch/x360_sdk'));import native_atomic_fixture as F
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave7/dac-terra');d=out/'timer-ctor-build';p=d/'Delay.obj';q=d/'TimerHandle.obj';b=F.Bundle([F.Coff(p,'?Delay_ctor@Delay@'),F.Coff(q,'??0TimerHandle@')]);b.select('?Delay_ctor@Delay@');errors=[];rng=random.Random(0xdd90);selfaddr=F.M.S.SELF
for i in range(128):
 img=bytearray(rng.randbytes(0x200));both=[]
 for compiled in (False,True):
  c=F.cpu(b);c.mem.wr(selfaddr,img);c.setg(3,selfaddr);trace=[]
  def filterctor(x):trace.append(('filter',x.g(3)&0xffffffff))
  def delayctor(x):trace.append(('delay',x.g(3)&0xffffffff))
  c.hooks[0x82b67d10]=filterctor;c.hooks[0x82b67ee8]=delayctor
  for at,n in b.unresolved.items():
   if n.startswith('?DelayFilter_ctor@'):c.hooks[at]=filterctor
   if n.startswith('??0DelayLine@'):c.hooks[at]=delayctor
  c.run(b.entry if compiled else 0x82b9dd90);obj=bytearray(c.mem.rd(selfaddr,0x200));assert F.cstring(c,struct.unpack_from('>I',obj,0xa4)[0])=='Unknown';obj[:4]=bytes(4);obj[0xa4:0xa8]=bytes(4);both.append((obj,trace,c.g(3)&0xffffffff))
 if both[0]!=both[1]:errors.append({'case':i,'diff':[hex(j) for j,(a,z) in enumerate(zip(both[0][0],both[1][0])) if a!=z][:24],'trace':[both[0][1],both[1][1]]})
r={'cases':128,'errors':errors,'sha256':hashlib.sha256(p.read_bytes()).hexdigest(),'timer_sha256':hashlib.sha256(q.read_bytes()).hexdigest(),'limits':'Delay constructor with real TimerHandle constructor, DelayFilter/DelayLine constructors controlled. Vtable and equal Unknown pointer normalized; no Delay runtime or hardware claim.'};(out/'timer-delay-create-comparison.json').write_text(json.dumps(r,indent=2)+'\n');print(len(errors),errors[:2]);assert not errors
