import sys,json,random,struct,hashlib
from pathlib import Path
root=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(root/'scratch/x360_sdk'));import native_atomic_fixture as F
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave7/dac-terra');p=out/'timer-ctor-build/TimerHandle.obj';b=F.Bundle([F.Coff(p,'??0TimerHandle@')]);b.select('??0TimerHandle@');errors=[];rng=random.Random(0xbea8);selfaddr=F.M.S.SELF
for i in range(128):
 img=bytearray(rng.randbytes(0x80));both=[]
 for compiled in (False,True):
  c=F.cpu(b);c.mem.wr(selfaddr,img);c.setg(3,selfaddr);c.run(b.entry if compiled else 0x82b6bea8);obj=bytearray(c.mem.rd(selfaddr,0x80));addr=struct.unpack_from('>I',obj,0xc)[0];assert F.cstring(c,addr)=='Unknown';obj[0xc:0x10]=bytes(4);both.append((obj,c.g(3)&0xffffffff))
 if both[0]!=both[1]:errors.append({'case':i,'diff':[hex(j) for j,(a,z) in enumerate(zip(both[0][0],both[1][0])) if a!=z]})
r={'cases':128,'errors':errors,'sha256':hashlib.sha256(p.read_bytes()).hexdigest(),'limits':'Actual TimerHandle constructor; equal Unknown string address normalized; no timer registration/hardware claim.'};(out/'timer-constructor-comparison.json').write_text(json.dumps(r,indent=2)+'\n');print(len(errors),errors[:2]);assert not errors
