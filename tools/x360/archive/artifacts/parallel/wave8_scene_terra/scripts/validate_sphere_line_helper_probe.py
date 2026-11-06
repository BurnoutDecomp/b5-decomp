import sys,random,json,struct,hashlib
from pathlib import Path
sys.path.insert(0,r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\scratch\x360_sdk');import native_atomic_fixture as F
sys.path.insert(0,r'D:\Reverse\BP-Decomp_Workflow\tools\re');import x360rd
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/parallel/wave8_scene_terra/volume-baseline');obj=out/'sphere-line-helper-probe.obj';b=F.Bundle([F.Coff(obj,'?rwcSphereLineSegIntersect@')]);b.select('?rwcSphereLineSegIntersect@');rng=random.Random(0x82ba81d8);A=0x50000000;O=0x50001000;bad=[];branches=set()
for i in range(256):
 def vec(v):return struct.pack('>4f',*v)
 if i%4==0:start,delta,centre,radius=(0,0,0,1),(1,0,0,2),(.1,0,0,3),1
 elif i%4==1:start,delta,centre,radius=(0,0,0,1),(-1,0,0,2),(2,0,0,3),.5
 elif i%4==2:start,delta,centre,radius=(0,0,0,1),(4,0,0,2),(2,0,0,3),.5
 else:start,delta,centre,radius=([rng.uniform(-5,5) for k in range(4)],[rng.uniform(-5,5) for k in range(4)],[rng.uniform(-5,5) for k in range(4)],rng.uniform(.01,3))
 v=[vec(x) for x in [start,delta,centre]];initial=rng.randbytes(0x40);r=[]
 for compiled in (False,True):
  c=F.cpu(b);c.mem.add(A,0x100);c.mem.add(O,0x40,initial)
  for k,x in enumerate(v):c.mem.wr(A+k*0x20,x)
  if not compiled:c.mem.add(0x82001000,0x1000,x360rd.rd(0x82001000,0x1000))
  c.setg(3,O);c.setg(4,A);c.setg(5,A+0x20);c.setg(6,A+0x40);c.setf(1,radius);c.run(b.entry if compiled else 0x82ba81d8);r.append((c.mem.rd(O,0x40),c.mem.rd(A,0x100),c.g(3)&0xffffffff))
 branches.add(r[0][2])
 if r[0]!=r[1]:bad.append([i,r[0][2],r[1][2],r[0][0][:8].hex(),r[1][0][:8].hex()])
print('cases 256 branches',branches,'mismatches',len(bad),bad[:3]);assert not bad and branches=={0,1,0xffffffff}
(out/'sphere-line-helper-probe-comparison.json').write_text(json.dumps({'cases':256,'branches':sorted(branches),'mismatches':bad,'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'temporary extracted sphere-line helper; finite targeted inside/receding/hit and random misses, all return branches, poisoned Fraction and input guards; NOT whole provider'},indent=2))
