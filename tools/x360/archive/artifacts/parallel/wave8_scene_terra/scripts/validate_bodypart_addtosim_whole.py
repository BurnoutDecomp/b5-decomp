from pathlib import Path
import sys,json,random,hashlib,struct
sys.path.insert(0,r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\scratch\x360_sdk');import native_atomic_fixture as F
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/parallel/wave8_scene_terra/bodypart-baseline');obj=out/'bodypart.obj';n='?AddToSim@PhysicalBodyPart@Deformation@BrnPhysics@@';o=F.Coff(obj,n);b=F.Bundle([o]);b.select(n);A=0x40000000;Q=A+0x800;T=A+0x1000;rng=random.Random(0x8260ad38);bad=[]
def run(comp,part,extent,inertia,velocity,scalars):
 c=F.cpu(b);c.mem.add(A,0x3000);c.mem.add(0x82f2a000,0x1000);c.mem.add(0x82fb9000,0x1000);c.mem.wr(A,part);c.mem.wr(0x82fb95b0,struct.pack('>4f',.2,.2,.2,0));calls=[];events=[]
 for frag,addr,val in zip(['kfPartLinearDrag','kfPartAngularDrag','kfPartMaxLinearVelocity','kfPartMaxAngularVelocity','kfPartMass','kfPartInertiaMultiplier'],[0x82f2a370,0x82f2a374,0x82f2a378,0x82f2a37c,0x82f2a380,0x82f2a384],scalars):
  c.mem.wr(addr,struct.pack('>f',val));c.mem.wr(F.address(b,frag),struct.pack('>f',val))
 c.mem.wr(F.address(b,'KV_PART_INERTIA_FLOOR'),struct.pack('>4f',.2,.2,.2,0))
 def ext(cpu):calls.append('extents');cpu.mem.wr(cpu.g(3)&0xffffffff,extent)
 def fat(cpu):calls.append(('fat',tuple(cpu.f(j) for j in range(1,5))));cpu.mem.wr(cpu.g(7)&0xffffffff,inertia)
 def queue(cpu):calls.append('queue');cpu.setg(3,Q)
 def add(cpu):calls.append('add');events.append(cpu.mem.rd(cpu.g(4)&0xffffffff,0xb4));cpu.setg(3,1)
 c.hooks[0x825e2ea0]=ext;c.hooks[F.address(b,'?CalculateAABBExtents@PhysicalBodyPart@')]=ext;c.hooks[0x82bc6c80]=fat;c.hooks[0x825bce08]=queue;c.hooks[0x825a3000]=add;c.hooks[F.address(b,'?AddEvent@?$BaseEventQueue@UInAddRigidBody')]=add
 for k,v in b.unresolved.items():
  if 'ComputeFatBoxInertia' in v:c.hooks[k]=fat
  elif 'GetAddRigidBodyQueue' in v:c.hooks[k]=queue
  elif 'Assert' in v:c.hooks[k]=lambda cpu:None
  elif v.startswith('__save') or v.startswith('__rest'):c.hooks[k]=lambda cpu:None
  elif v=='memcpy':
   def copy(cpu):cpu.mem.wr(cpu.g(3)&0xffffffff,cpu.mem.rd(cpu.g(4)&0xffffffff,cpu.g(5)&0xffffffff))
   c.hooks[k]=copy
 c.setg(3,A);c.setg(4,Q);c.setg(5,T);c.vr[1]=struct.unpack('>4I',velocity[:16]);c.vr[2]=struct.unpack('>4I',velocity[16:]);c.run(b.entry if comp else 0x8260ad38)
 # Compare initialized fields only, avoiding original stack gaps.
 e=events[0];mask=e[:8]+e[0x10:0x98]+e[0xa0:0xa1]+e[0xb0:0xb4]
 return c.mem.rd(A,0xa0).hex(),calls,mask.hex()
for i in range(128):
 rnd=lambda n:struct.pack('>'+str(n)+'f',*[rng.uniform(.25,8) for _ in range(n)])
 part=bytearray(rng.randbytes(0x200));part[:0x60]=rnd(24);extent=rnd(4);inertia=rnd(4);velocity=rnd(8);scalars=[.2,.3,30.,10.,rng.uniform(1,100),rng.uniform(.1,3)]
 x=run(False,part,extent,inertia,velocity,scalars);y=run(True,part,extent,inertia,velocity,scalars)
 if x!=y:bad.append({'case':i,'original':x,'compiled':y});break
print('mismatches',len(bad),bad[:1]);(out/'addtosim-whole-comparison.json').write_text(json.dumps({'cases':128,'mismatches':bad,'scope':'whole AddToSim finite controlled extents, fat-box inertia and queue; initialized event fields, all part 0xa0 bytes and call trace; excludes uncontrolled services, nonfinite and hardware','object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest()},indent=2)+'\n');assert not bad
