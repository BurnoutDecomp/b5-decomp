from pathlib import Path
import sys,json,random,struct,hashlib
sys.path.insert(0,r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\scratch\x360_sdk');import native_atomic_fixture as F
base=Path('D:/Reverse/BP-X360-Experimental-Artifacts/parallel/wave8_scene_terra');out=base/'geometry-mask-closure';obj=base/'box-real-mask/box.obj';probe=out/'box_bool_consumer.obj';n3='?BoxOverlappingTest@CgsGeometric@@YA?AVMaskScalar@vpu@math@rw@@ABUBox@1@0VVector3@345@@Z';n2='?BoxOverlappingTest@CgsGeometric@@YA?AVMaskScalar@vpu@math@rw@@ABUBox@1@0@Z';names=['?BoxBoolConsumer@@',n2,n3,'?GetBoxExtentsAlongAxis@CgsGeometric@@'];b=F.Bundle([F.Coff(probe,names[0])]+[F.Coff(obj,n) for n in names[1:]]);b.select(names[0]);A=0x40000000;rng=random.Random(0x8261AFF4);bad=[]
class Stop(Exception):pass
def box(p,d):return b''.join(struct.pack('>4f',*row) for row in [(1.,0.,0.,0.),(0.,1.,0.,0.),(0.,0.,1.,0.),(*p,0.),(*d,0.)])
def run(comp,x,y):
 c=F.cpu(b);c.mem.add(A,0x1000);c.mem.wr(A,x);c.mem.wr(A+0x100,y);c.setg(3,A);c.setg(4,A+0x100)
 if comp:c.run(b.entry);return c.g(3)&0xff
 c.run(0x825BF090);c.vr[127]=(0,0,0,0);res=[]
 def stop(value):
  def f(cpu):res.append(value);raise Stop()
  return f
 def observe(pc,word):
  if pc==0x8261B00C:res.append(0);raise Stop()
  if pc==0x8261B020:res.append(1);raise Stop()
 c.instruction_observer=observe
 try:c.run(0x8261AFF8)
 except Stop:pass
 assert res;return res[0]
for i in range(128):
 p1=[float(rng.randrange(-5,6)) for j in range(3)];p2=[float(rng.randrange(-5,6)) for j in range(3)];d1=[float(rng.randrange(1,5)) for j in range(3)];d2=[float(rng.randrange(1,5)) for j in range(3)];x=box(p1,d1);y=box(p2,d2);a=run(False,x,y);z=run(True,x,y)
 if a!=z:bad.append({'case':i,'original':a,'compiled':z})
r={'cases':128,'mismatches':bad,'scope':'actual linked compiled BoxBoolConsumer -> real BoxOverlappingTest -> MaskScalar::GetBool versus original full BoxOverlappingTest 825BF090 and actual caller bool branch 8261AFF8..B020; axis-aligned finite integer boxes only; no hardware or full caller claim','box_object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'consumer_object_sha256':hashlib.sha256(probe.read_bytes()).hexdigest()};(out/'box-bool-comparison.json').write_text(json.dumps(r,indent=2)+'\n');print('mismatches',len(bad),bad[:1]);assert not bad
