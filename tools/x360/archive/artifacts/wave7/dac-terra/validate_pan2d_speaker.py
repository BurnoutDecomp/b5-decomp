import sys,json,random,struct,hashlib,math
from pathlib import Path
root=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(root/'scratch/x360_sdk'));import native_atomic_fixture as F
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave7/dac-terra');p=out/'pan2d-build/Pan2D.obj';b=F.Bundle([F.Coff(p,'?SpeakerConfig@Pan2D@')]);b.select('?SpeakerConfig@Pan2D@');errors=[];rng=random.Random(0x85d0);selfaddr=F.M.S.SELF
for i in range(32):
 initial=bytearray(rng.randbytes(0x200));initial[0xec]=i&1;struct.pack_into('>f',initial,0x64,20.+i%20);struct.pack_into('>f',initial,0x68,110.+i%20);both=[]
 for compiled in (False,True):
  c=F.cpu(b);c.mem.wr(selfaddr,initial);c.setg(3,selfaddr);trace=[]
  def sin(x):x.setf(1,math.sin(x.f(1)))
  def cos(x):x.setf(1,math.cos(x.f(1)))
  c.hooks[0x82c095d0]=sin;c.hooks[0x82c096a0]=cos
  for at,n in b.unresolved.items():
   if n=='sin':c.hooks[at]=sin
   if n=='cos':c.hooks[at]=cos
   if n.startswith(('__savefpr_', '__restfpr_')):c.hooks[at]=lambda x:None
  c.run(b.entry if compiled else 0x82b985d0);both.append(c.mem.rd(selfaddr,0x200))
 if both[0]!=both[1]:errors.append({'case':i,'diff':[hex(j) for j,(a,z) in enumerate(zip(both[0],both[1])) if a!=z][:40]})
r={'cases':32,'errors':errors,'sha256':hashlib.sha256(p.read_bytes()).hexdigest(),'limits':'Actual SpeakerConfig and ComputeTransform; sin/cos and FPR save/restore controlled; finite geometry only; no pan audio/hardware claim.'};(out/'pan2d-speaker-comparison.json').write_text(json.dumps(r,indent=2)+'\n');print(len(errors),errors[:2]);assert not errors
