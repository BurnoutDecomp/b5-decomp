import sys,json,random,struct,hashlib
from pathlib import Path
root=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(root/'scratch/x360_sdk'));import native_atomic_fixture as F
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave7/dac-terra');d=out/'delay-native-build';objs=[d/'Delay.obj',d/'DelayFilter.obj',d/'DelayLine.obj',d/'TimerHandle.obj'];b=F.Bundle([F.Coff(objs[0],'??0Delay@'),F.Coff(objs[1],'??0DelayFilter@'),F.Coff(objs[2],'??0DelayLine@'),F.Coff(objs[3],'??0TimerHandle@')]);b.select('??0Delay@');errors=[];rng=random.Random(0xdd90);selfaddr=F.M.S.SELF
orig={'filter':0x82b67d10,'delayline':0x82b67ee8,'timer':0x82b6bea8};comp={x:F.address(b,n) for x,n in [('filter','??0DelayFilter@'),('delayline','??0DelayLine@'),('timer','??0TimerHandle@')]}
for i in range(128):
 img=bytearray(rng.randbytes(0x300));both=[]
 for compiled in (False,True):
  c=F.cpu(b);c.mem.wr(selfaddr,img);c.setg(3,selfaddr);trace=[];addrs=comp if compiled else orig
  def observe(pc,word):
   for n,a in addrs.items():
    if pc==a:trace.append((n,c.g(3)&0xffffffff))
  c.instruction_observer=observe;c.run(b.entry if compiled else 0x82b9dd90);obj=bytearray(c.mem.rd(selfaddr,0x300));assert F.cstring(c,struct.unpack_from('>I',obj,0xa4)[0])=='Unknown';obj[:4]=bytes(4);obj[0xa4:0xa8]=bytes(4);both.append((obj,trace,c.g(3)&0xffffffff))
 if both[0]!=both[1]:errors.append({'case':i,'diff':[hex(j) for j,(a,z) in enumerate(zip(both[0][0],both[1][0])) if a!=z][:24], 'trace':[both[0][1],both[1][1]]})
r={'cases':128,'errors':errors,'objects':{p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in objs},'limits':'Actual Delay constructor and real DelayFilter/DelayLine/TimerHandle nested constructors, call order, state and return; vtable/Unknown pointer normalized. No allocator/hardware claim.'};(out/'delay-constructor-comparison.json').write_text(json.dumps(r,indent=2)+'\n');print(len(errors),errors[:1]);assert not errors
