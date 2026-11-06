import sys,json,random,struct,hashlib
from pathlib import Path
root=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(root/'scratch/x360_sdk'));import native_atomic_fixture as F
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave7/dac-terra');p=out/'profiler-native-build/Profiler.obj';b=F.Bundle([F.Coff(p,'?End@Profiler@')]);b.select('?End@Profiler@');errors=[];rng=random.Random(0xa9f8);selfaddr=F.M.S.SELF;aux=F.M.S.AUX
for i in range(128):
 img=bytearray(rng.randbytes(0x100));struct.pack_into('>d',img,8,[0.,5.,20.][i%3]);struct.pack_into('>f',img,0x10,0.5);struct.pack_into('>I',img,0x14,i*11);struct.pack_into('>I',img,0x18,10);struct.pack_into('>I',img,0x1c,1000);struct.pack_into('>I',img,0x20,500 if i&1 else 5000);struct.pack_into('>I',img,0x24,10);struct.pack_into('>I',img,0x28,(i//2)&1);struct.pack_into('>I',img,0x2c,0);struct.pack_into('>I',img,0x30,aux+0x1000);now=[0.,6.,15.][i%3];cycle=2000+i*37;both=[]
 for compiled in (False,True):
  c=F.cpu(b);c.mem.wr(selfaddr,img);c.mem.wr(aux+0x1008,struct.pack('>d',now));c.setg(3,selfaddr);trace=[]
  def sample(x):trace.append('cycle');x.setg(3,cycle)
  c.hooks[0x82b64558]=sample
  for at,n in b.unresolved.items():
   if 'GetCpuCycle' in n:c.hooks[at]=sample
   if n.startswith(('__savefpr_', '__restfpr_')):c.hooks[at]=lambda x:None
  c.run(b.entry if compiled else 0x82b6a9f8);both.append((c.mem.rd(selfaddr,0x100),trace))
 if both[0]!=both[1]:errors.append({'case':i,'diff':[hex(j) for j,(a,z) in enumerate(zip(both[0][0],both[1][0])) if a!=z][:25]})
r={'cases':128,'errors':errors,'sha256':hashlib.sha256(p.read_bytes()).hexdigest(),'limits':'Actual Profiler End with real typed System time field; GetCpuCycle and FPR helpers controlled, finite time/cycle cases; no hardware/concurrency claim.'};(out/'profiler-end-comparison.json').write_text(json.dumps(r,indent=2)+'\n');print(len(errors),errors[:2]);assert not errors
