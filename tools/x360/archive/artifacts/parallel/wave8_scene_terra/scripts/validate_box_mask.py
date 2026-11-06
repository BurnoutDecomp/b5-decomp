from pathlib import Path
import sys,json,random,struct,hashlib
sys.path.insert(0,r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\scratch\x360_sdk');import native_atomic_fixture as F
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/parallel/wave8_scene_terra/box-real-mask');obj=out/'box.obj';names=['?BoxOverlappingTest@CgsGeometric@@YA?AVMaskScalar@vpu@math@rw@@ABUBox@1@0VVector3@345@@Z','?BoxOverlappingTest@CgsGeometric@@YA?AVMaskScalar@vpu@math@rw@@ABUBox@1@0@Z','?GetBoxExtentsAlongAxis@CgsGeometric@@'];b=F.Bundle([F.Coff(obj,n) for n in names]);A=0x40000000;rng=random.Random(0x825BF090);bad=[]
def box(p,d):return b''.join(struct.pack('>4f',*row) for row in [(1.,0.,0.,0.),(0.,1.,0.,0.),(0.,0.,1.,0.),(*p,0.),(*d,0.)])
def run(comp,which,b1,b2,axis):
 b.select(names[which]);c=F.cpu(b);c.mem.add(A,0x1000);c.mem.wr(A,b1);c.mem.wr(A+0x100,b2);c.setg(3,A);c.setg(4,A+0x100);c.vr[1]=axis;c.run(b.entry if comp else (0x825BEFA0 if which==0 else 0x825BF090));return list(c.vr[1])
for i in range(128):
 p1=[float(rng.randrange(-5,6)) for _ in range(3)];p2=[float(rng.randrange(-5,6)) for _ in range(3)];d1=[float(rng.randrange(1,5)) for _ in range(3)];d2=[float(rng.randrange(1,5)) for _ in range(3)];a=box(p1,d1);z=box(p2,d2);idx=i%3;axis=tuple(int.from_bytes(struct.pack('>f',1. if j==idx else 0.),'big') for j in range(4))
 for which in [0,1]:
  x=run(False,which,a,z,axis);y=run(True,which,a,z,axis)
  if x!=y:bad.append({'case':i,'which':which,'original':x,'compiled':y})
r={'cases_per_overload':128,'mismatches':bad,'scope':'real whole compiled BoxOverlappingTest 3-arg and 2-arg including compiled GetBoxExtents/And, compared v1 mask lanes to original 825BEFA0/825BF090 for axis-aligned finite integer boxes and cardinal axes; not proof of arbitrary floating precision, nonfinite cases or all geometry','object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest()};(out/'box-overlap-comparison.json').write_text(json.dumps(r,indent=2)+'\n');print('mismatches',len(bad),bad[:2]);assert not bad
