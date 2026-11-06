import sys,json,random,struct,hashlib,math
from pathlib import Path
root=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(root/'scratch/x360_sdk'));import native_atomic_fixture as F
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave7/dac-terra');p=out/'pan2d1-build/Pan2D1.obj';b=F.Bundle([F.Coff(p,'?CreateInstance@Pan2D1@')]);b.select('?CreateInstance@Pan2D1@');errors=[];rng=random.Random(0x3540);selfaddr=F.M.S.SELF;aux=F.M.S.AUX
for i in range(128):
 initial=bytearray(rng.randbytes(0x300));initial[0x20]=1+i%6;initial[0x21]=1+(i//6)%6;both=[]
 for compiled in (False,True):
  c=F.cpu(b);c.mem.wr(selfaddr,initial);c.mem.wr(aux+0x1000,struct.pack('>3f',30.+i%5,110.+i%5,float(i%4)));c.setg(3,selfaddr);c.setg(4,aux+0x1000 if i&1 else 0);trace=[]
  def sin(x):x.setf(1,math.sin(x.f(1)))
  def cos(x):x.setf(1,math.cos(x.f(1)))
  c.hooks[0x82c095d0]=sin;c.hooks[0x82c096a0]=cos
  for at,n in b.unresolved.items():
   if n=='sin':c.hooks[at]=sin
   if n=='cos':c.hooks[at]=cos
   if n.startswith(('__savefpr_', '__restfpr_')):c.hooks[at]=lambda x:None
  c.run(b.entry if compiled else 0x82ba3540);obj=bytearray(c.mem.rd(selfaddr,0x300));obj[:4]=bytes(4);both.append((obj,trace,c.g(3)&0xffffffff))
 if both[0]!=both[1]:errors.append({'case':i,'diff':[hex(j) for j,(a,z) in enumerate(zip(both[0][0],both[1][0])) if a!=z][:32],'trace':[both[0][1],both[1][1]]})
r={'cases':128,'errors':errors,'sha256':hashlib.sha256(p.read_bytes()).hexdigest(),'limits':'Actual constructor, actual SpeakerConfig; sin/cos and FPR save/restore controlled, finite optional config, input/output channels 1–6. No pan DSP/hardware claim.'};(out/'pan2d1-create-speaker-comparison.json').write_text(json.dumps(r,indent=2)+'\n');print(len(errors),errors[:2]);assert not errors
