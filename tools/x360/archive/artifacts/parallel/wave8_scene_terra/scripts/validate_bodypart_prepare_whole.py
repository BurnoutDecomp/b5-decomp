from pathlib import Path
import sys,json,random,hashlib,struct
sys.path.insert(0,r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\scratch\x360_sdk');import native_atomic_fixture as F
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/parallel/wave8_scene_terra/bodypart-baseline');obj=out/'bodypart.obj';name='?Prepare@PhysicalBodyPart@Deformation@BrnPhysics@@';b=F.Bundle([F.Coff(obj,name)]);b.select(name);A=0x40000000;IK=A+0x400;SPEC=A+0x600;G=A+0x900;V=A+0xa00;rng=random.Random(0x82626700);bad=[]
def run(comp,part,orient,gfx,veh,com,id64,id32):
 c=F.cpu(b);c.mem.add(A,0x3000);c.mem.wr(A,part);c.mem.wr(IK+8,SPEC.to_bytes(4,'big'));c.mem.wr(SPEC+0x40,orient);c.mem.wr(G,gfx);c.mem.wr(V,veh);calls=[];fires=[]
 def construct(cpu):calls.append(('construct',cpu.g(3)&0xffffffff))
 def calc(cpu):calls.append(('calc',cpu.g(3)&0xffffffff,cpu.mem.rd(cpu.g(4)&0xffffffff,64).hex()));cpu.mem.wr(A+0x180,com)
 def fire(cpu):fires.append(1)
 c.hooks[0x825A1598]=construct;c.hooks[0x8260ABB0]=calc;c.hooks[F.address(b,"?CalcBoundingBox@PhysicalBodyPart@")]=calc
 for nm,addr in [('BeginAssert',0x82817548),('FireAssert',0x82820810),('EndAssert',0x82817558)]:
  fn=fire if nm=='FireAssert' else lambda cpu:None;c.hooks[addr]=fn
  for k,v in b.unresolved.items():
   if nm in v:c.hooks[k]=fn
 for k,v in b.unresolved.items():
  if '?Construct@ExternalPhysicsBody' in v:c.hooks[k]=construct
  elif '?CalcBoundingBox@PhysicalBodyPart' in v:c.hooks[k]=calc
  elif v.startswith('__save') or v.startswith('__rest'):c.hooks[k]=lambda cpu:None
 c.setg(3,A);c.setg(4,id64);c.setg(5,id32);c.setg(6,A+0x800);c.setg(7,IK);c.setg(8,G);c.setg(9,V);c.run(b.entry if comp else 0x82626700);return calls,len(fires),c.mem.rd(A,0x1f0).hex()
for i in range(128):
 def rnd(n):return struct.pack('>'+str(n)+'f',*[rng.uniform(-5,5) for j in range(n)])
 part=rng.randbytes(0x1f0);orient=rnd(16);gfx=rnd(16);veh=rnd(16);com=rnd(4);id64=rng.getrandbits(64);id32=rng.getrandbits(32);x=run(False,part,orient,gfx,veh,com,id64,id32);y=run(True,part,orient,gfx,veh,com,id64,id32)
 if x!=y:bad.append({'case':i,'original':x,'compiled':y});break
r={'cases':128,'mismatches':bad,'scope':'whole original Prepare82626700 vs exact bounded actual source/real headers; external body Construct, CalcBoundingBox and assertion services controlled, finite arbitrary transform xyzw, all0x1f0 part bytes, call sequence/receiver/vehicle matrix and assertion count. No real construction/CalcBoundingBox/full owner TU/metadata/hardware claim','object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest()};(out/'prepare-whole-comparison.json').write_text(json.dumps(r,indent=2)+'\n');print('cases',128,'mismatches',len(bad),bad[:1]);assert not bad
