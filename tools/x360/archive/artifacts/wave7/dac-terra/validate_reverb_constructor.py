import sys,json,random,struct,hashlib
from pathlib import Path
root=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(root/'scratch/x360_sdk'));import native_atomic_fixture as F
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave7/dac-terra');d=out/'reverb-native-build';p=d/'ReverbModel1.obj';objs=[p,d/'ReverbFilters.obj',d/'TimerHandle.obj',d/'DelayLine.obj'];b=F.Bundle([F.Coff(objs[0],'??0ReverbModel1@'),F.Coff(objs[1],'??0AllPassFilter@'),F.Coff(objs[2],'??0TimerHandle@'),F.Coff(objs[3],'??0DelayLine@')]);b.select('??0ReverbModel1@');errors=[];rng=random.Random(0xe938);selfaddr=F.M.S.SELF
orig={'allpass':0x82b6c3d8,'delay':0x82b67ee8,'timer':0x82b6bea8,'comb':0x82b6c6b0};comp={x:F.address(b,n) for x,n in [('allpass','??0AllPassFilter@'),('delay','??0DelayLine@'),('timer','??0TimerHandle@'),('comb','??0CombFilter@')]}
for i in range(128):
 img=bytearray(rng.randbytes(0x600));both=[]
 for compiled in (False,True):
  c=F.cpu(b);c.mem.wr(selfaddr,img);c.setg(3,selfaddr);trace=[];addrs=comp if compiled else orig
  def observe(pc,word):
   for n,a in addrs.items():
    if pc==a:trace.append((n,c.g(3)&0xffffffff))
  c.instruction_observer=observe;c.run(b.entry if compiled else 0x82b9e938);obj=bytearray(c.mem.rd(selfaddr,0x600));addr=struct.unpack_from('>I',obj,0x148)[0];assert F.cstring(c,addr)=='Unknown';obj[:4]=bytes(4);obj[0x148:0x14c]=bytes(4);both.append((obj,trace,c.g(3)&0xffffffff))
 if both[0]!=both[1]:errors.append({'case':i,'diff':[hex(j) for j,(a,z) in enumerate(zip(both[0][0],both[1][0])) if a!=z][:32], 'trace':[both[0][1],both[1][1]],'returns':[both[0][2],both[1][2]]})
r={'cases':128,'errors':errors,'objects':{p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in objs},'limits':'Actual ReverbModel1 constructor and all nested AllPass/DelayLine/Timer/Comb constructors; their real call order/addresses, full storage and return compared. Vtable and equal Unknown string addresses normalized; no allocation/hardware claim.'};(out/'reverb-constructor-comparison.json').write_text(json.dumps(r,indent=2)+'\n');print(len(errors),errors[:1]);assert not errors
