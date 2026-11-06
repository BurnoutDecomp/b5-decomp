from pathlib import Path
import sys,json,random,hashlib,struct
sys.path.insert(0,r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\scratch\x360_sdk');import native_atomic_fixture as F
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/parallel/wave8_scene_terra/deformation-next');obj=out/'9.obj';n='?CompressSelectedRig@DeformationDebugComponent@';o=F.Coff(obj,n);b=F.Bundle([o]);b.select(n);A=0x40000000;R=A+0x1000;SPEC=A+0x10000;VEH=A+0x11000;SS=A+0x12000;LOCAL=A+0x14000;WORLD=A+0x15000;rng=random.Random(0x825df030);bad=[]
def run(comp,init,ss,local,world,vehicle,scales):
 c=F.cpu(b);c.mem.add(A,0x20000);c.mem.wr(A,init);c.mem.wr(A+0x14,R.to_bytes(4,'big'));c.mem.wr(R+0x18e0,SPEC.to_bytes(4,'big'));c.mem.wr(R+0x194c,VEH.to_bytes(4,'big'));c.mem.wr(SPEC+0x652,b'\x01');c.mem.wr(R+0x1954,SS.to_bytes(4,'big'));c.mem.wr(R+0x1950+0x19c,LOCAL.to_bytes(4,'big'));c.mem.wr(R+0x1950+0x1a0,WORLD.to_bytes(4,'big'));c.mem.wr(SS,ss);c.mem.wr(LOCAL,local);c.mem.wr(WORLD,world);c.mem.wr(VEH+0x10,vehicle)
 for k,v in b.unresolved.items():
  if 'GetTransform@DeformableObject' in v:c.hooks[k]=lambda cpu:cpu.mem.wr(cpu.g(4)&0xffffffff,vehicle[:64])
  elif v.startswith('__save') or v.startswith('__rest'):c.hooks[k]=lambda cpu:None
 for i,x in enumerate(scales):c.setf(i+1,x)
 c.setg(3,A);c.run(b.entry if comp else 0x825df030);return c.mem.rd(A+0x3c,24).hex(),c.mem.rd(LOCAL,16).hex(),c.mem.rd(WORLD,16).hex()
for i in range(128):
 rnd=lambda n:struct.pack('>'+str(n)+'f',*[rng.uniform(-5,5) for _ in range(n)])
 init=rng.randbytes(0x100);ss=rnd(10)+rng.randbytes(24);local=rnd(4);world=rnd(4);vehicle=rnd(16);scales=[rng.uniform(-2,2) for _ in range(6)]
 x=run(False,init,ss,local,world,vehicle,scales);y=run(True,init,ss,local,world,vehicle,scales)
 if x!=y:bad.append({'i':i,'original':x,'compiled':y});break
print('mismatches',len(bad),bad[:1]);(out/'compress-comparison.json').write_text(json.dumps({'cases':128,'mismatches':bad,'scope':'whole CompressSelectedRig 825DF030 vs actual whole object, one sensor and controlled GetTransform; finite scales, matrix, vector xyzW and preserved radius, compares sliders and both sphere bytes; excludes real transform provider/nonfinite/hardware','object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest()},indent=2)+'\n');assert not bad
