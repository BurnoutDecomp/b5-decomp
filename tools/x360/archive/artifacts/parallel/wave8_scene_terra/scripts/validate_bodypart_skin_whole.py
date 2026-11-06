from pathlib import Path
import sys,json,random,hashlib,struct
sys.path.insert(0,r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\scratch\x360_sdk');import native_atomic_fixture as F
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/parallel/wave8_scene_terra/bodypart-baseline');obj=out/'bodypart.obj';n='?CalculateSkinnedPoint@PhysicalBodyPart@Deformation@BrnPhysics@@';b=F.Bundle([F.Coff(obj,n)]);b.select(n);A=0x40000000;IK=A+0x300;SPEC=A+0x500;TAGS=A+0x1000;POINT=A+0x1800;rng=random.Random(0x825E2560);bad=[]
def run(comp,vertex,weights,positions,initial):
 c=F.cpu(b);c.mem.add(A,0x4000);c.mem.wr(A+0x1dc,IK.to_bytes(4,'big'));c.mem.wr(IK+4,TAGS.to_bytes(4,'big'));c.mem.wr(IK+8,SPEC.to_bytes(4,'big'));c.mem.wr(SPEC+0x1d8,(3).to_bytes(4,'big'));c.mem.wr(POINT,vertex+weights+bytes([0,1,2,0]));c.mem.wr(A+0x2000,b'\xA5'*48)
 for j in range(3):
  t=TAGS+32*j;sp=A+0x1200+0x80*j;c.mem.wr(t,positions[j]);c.mem.wr(t+0x10,sp.to_bytes(4,'big'));c.mem.wr(sp+0x20,initial[j])
 fires=[]
 def fire(cpu):fires.append(1)
 for nm,addr in [('BeginAssert',0x82817548),('FireAssert',0x82820810),('EndAssert',0x82817558)]:
  fn=fire if nm=='FireAssert' else lambda cpu:None;c.hooks[addr]=fn
  for k,v in b.unresolved.items():
   if nm in v:c.hooks[k]=fn
 for k,v in b.unresolved.items():
  if v.startswith('__save') or v.startswith('__rest'):c.hooks[k]=lambda cpu:None
 c.setg(3,A+0x2010);c.setg(4,A);c.setg(5,POINT);c.run(b.entry if comp else 0x825E2560);return c.mem.rd(A+0x2000,48).hex(),len(fires)
for i in range(128):
 def rnd():return struct.pack('>4f',*[rng.randrange(-8,9)*.25 for j in range(4)])
 vertex=rnd();weights=struct.pack('>3f',*[rng.randrange(-4,5)*.25 for j in range(3)]);positions=[rnd() for j in range(3)];initial=[rnd() for j in range(3)];x=run(False,vertex,weights,positions,initial);y=run(True,vertex,weights,positions,initial)
 if x!=y:bad.append({'case':i,'original':x,'compiled':y})
r={'cases':128,'mismatches':bad,'scope':'whole original CalculateSkinnedPoint825E2560 vs exact bounded actual source/real headers, three valid TagPoint influences, finite quarter-step xyzw/weights; compares output xyzw and guards, assertion count, actual data access. No IKDrivenPoint arm, arbitrary rounding/nonfinite, full owner TU or hardware claim','object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest()};(out/'skin-whole-comparison.json').write_text(json.dumps(r,indent=2)+'\n');print('cases',128,'mismatches',len(bad),bad[:1]);assert not bad
