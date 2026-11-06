import sys,json,random,struct,hashlib
from pathlib import Path
root=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(root/'scratch/x360_sdk'));import native_atomic_fixture as F
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave7/dac-terra');p=out/'pan2d-build/Pan2D.obj';b=F.Bundle([F.Coff(p,'?CreateInstance@Pan2D@')]);b.select('?CreateInstance@Pan2D@');errors=[];rng=random.Random(0x34a0);selfaddr=F.M.S.SELF;aux=F.M.S.AUX
for i in range(128):
 initial=bytearray(rng.randbytes(0x200));both=[]
 for compiled in (False,True):
  c=F.cpu(b);c.mem.wr(selfaddr,initial);c.mem.wr(aux+0x1000,struct.pack('>2f',30.+i%5,110.+i%5));c.setg(3,selfaddr);c.setg(4,aux+0x1000 if i&1 else 0);trace=[]
  def speaker(x):trace.append(x.g(3)&0xffffffff);x.setg(3,1)
  c.hooks[0x82b985d0]=speaker;c.hooks[F.address(b,'?SpeakerConfig@Pan2D@')]=speaker
  c.run(b.entry if compiled else 0x82ba34a0);obj=bytearray(c.mem.rd(selfaddr,0x200));obj[:4]=bytes(4);both.append((obj,trace,c.g(3)&0xffffffff))
 if both[0]!=both[1]:errors.append({'case':i,'diff':[hex(j) for j,(a,z) in enumerate(zip(both[0][0],both[1][0])) if a!=z][:32],'trace':[both[0][1],both[1][1]]})
r={'cases':128,'errors':errors,'sha256':hashlib.sha256(p.read_bytes()).hexdigest(),'limits':'Actual constructor with controlled SpeakerConfig; derived vtable address normalized, finite optional config. No pan DSP/hardware claim.'};(out/'pan2d-create-comparison.json').write_text(json.dumps(r,indent=2)+'\n');print(len(errors),errors[:2]);assert not errors
