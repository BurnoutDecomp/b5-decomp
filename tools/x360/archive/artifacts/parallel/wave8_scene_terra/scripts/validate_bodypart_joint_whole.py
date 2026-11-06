from pathlib import Path
import sys,json,random,hashlib,struct
sys.path.insert(0,r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\scratch\x360_sdk');import native_atomic_fixture as F
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/parallel/wave8_scene_terra/bodypart-baseline');obj=out/'bodypart.obj';n='?UpdateJoint@PhysicalBodyPart@Deformation@BrnPhysics@@';o=F.Coff(obj,n);b=F.Bundle([o]);b.select(n);A=0x40000000;IK=A+0x300;OBJ=A+0x1000;VEH=A+0x3000;SPEC=A+0x4000;JOINT=A+0x4500;TAG=A+0x5000;TAGSPEC=A+0x5100
box=F.address(b,'?CalcBoundingBox@PhysicalBodyPart@');tag=F.address(b,'?GetTagPoint@IKBodyPart@');rng=random.Random(0x8260b0f8);bad=[]
def run(comp,part,vehicle,joint,tagdata,rest,dt):
 c=F.cpu(b);c.mem.add(A,0x8000);c.mem.add(0x82fb9000,0x1000);c.mem.wr(A,part);c.mem.wr(A+0x1dc,IK.to_bytes(4,'big'));c.mem.wr(A+0x1e0,OBJ.to_bytes(4,'big'));c.mem.wr(A+0x1e4,b'\x01');c.mem.wr(A+0x1e8,b'\x00');c.mem.wr(IK+8,SPEC.to_bytes(4,'big'));c.mem.wr(IK+14,b'\0');c.mem.wr(SPEC+0x1c0,JOINT.to_bytes(4,'big'));c.mem.wr(OBJ+0x194c,VEH.to_bytes(4,'big'));c.mem.wr(VEH+0x10,vehicle);c.mem.wr(JOINT,joint);c.mem.wr(TAG,tagdata);c.mem.wr(TAG+0x10,TAGSPEC.to_bytes(4,'big'));c.mem.wr(TAGSPEC+0x20,rest);trace=[]
 for addr,val in [(0x82fb9ac0,9.8100004),(0x82fb95f0,.1),(0x82fb9750,1.),(0x82fb9d60,3.),(0x82fb9e30,1.)]:c.mem.wr(addr,struct.pack('>4f',*[val]*4))
 def boxhook(cpu):trace.append(('box',cpu.g(3)&0xffffffff,cpu.mem.rd(cpu.g(4)&0xffffffff,64).hex()))
 def taghook(cpu):trace.append(('tag',cpu.g(4)&0xffffffff));cpu.setg(3,TAG)
 c.hooks[0x8260abb0]=boxhook;c.hooks[box]=boxhook;c.hooks[0x825c1170]=taghook;c.hooks[tag]=taghook
 for nm,addr in [('BeginAssert',0x82817548),('FireAssert',0x82820810),('EndAssert',0x82817558)]:
  fn=lambda cpu,nm=nm:trace.append((nm,));c.hooks[addr]=fn
  for k,v in b.unresolved.items():
   if nm in v:c.hooks[k]=fn
 for k,v in b.unresolved.items():
  if 'GetActiveJointIndex@' in v:c.hooks[k]=lambda cpu:cpu.setg(3,0)
  if 'GetActiveJointSpec@' in v:c.hooks[k]=lambda cpu:cpu.setg(3,JOINT)
  if v.startswith('__save') or v.startswith('__rest'):c.hooks[k]=lambda cpu:None
 q=struct.unpack('>I',struct.pack('>f',dt))[0];c.vr[1]=(q,)*4;c.setg(3,A);c.run(b.entry if comp else 0x8260b0f8);return c.mem.rd(A,0x1f0).hex(),trace
for i in range(128):
 def rnd(n):return struct.pack('>'+str(n)+'f',*[rng.randrange(-8,9)*.125 for _ in range(n)])
 part=bytearray(rng.randbytes(0x1f0));part[:0x60]=rnd(24);part[0x160:0x1d0]=rnd(28);vehicle=rnd(24);joint=rnd(16);tagdata=rnd(4);rest=rnd(4);dt=rng.randrange(1,16)*.005
 x=run(False,part,vehicle,joint,tagdata,rest,dt);y=run(True,part,vehicle,joint,tagdata,rest,dt)
 if x!=y:bad.append({'case':i,'original':x,'compiled':y})
print('mismatches',len(bad),bad[:1]);(out/'joint-whole-comparison.json').write_text(json.dumps({'cases':128,'mismatches':bad,'scope':'whole actual compiled UpdateJoint against original, controlled CalcBoundingBox, tag and assert services; random finite quarter-step state, compares full 0x1f0 object bytes and service trace; no real service, nonfinite or hardware claim','object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest()},indent=2)+'\n');assert not bad
