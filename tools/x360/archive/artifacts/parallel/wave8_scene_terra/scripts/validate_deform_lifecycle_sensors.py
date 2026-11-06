from pathlib import Path
import sys,json,random,struct,hashlib
sys.path.insert(0,r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\scratch\x360_sdk');import native_atomic_fixture as F
p=Path('D:/Reverse/BP-X360-Experimental-Artifacts/parallel/wave8_scene_terra/deformation-next');obj=p/'13.obj';n='?ResetSensors@DeformableObject@';o=F.Coff(obj,n);b=F.Bundle([o]);b.select(n);A=0x40000000;SPEC=A+0xa000;VEH=A+0xb000;rng=random.Random(0x82623d60);bad=[]
def run(comp,transform,velocity,wheels,scales,init,enabled):
 c=F.cpu(b);c.mem.add(A,0x10000);c.mem.wr(A,init);c.mem.wr(A+0x18e0,SPEC.to_bytes(4,'big'));c.mem.wr(A+0x194c,VEH.to_bytes(4,'big'));c.mem.wr(A+0x672a,bytes([enabled]));c.mem.wr(SPEC+0x652,b'\x01');c.mem.wr(SPEC+0x110+0x32,b'\x01');c.mem.wr(VEH+0x10,transform);c.mem.wr(VEH+0x50,velocity)
 for i in range(4):c.mem.wr(VEH+0x1b0+0xe0*i,wheels[i]);c.mem.wr(SPEC+0x60+0x30*i,scales[i])
 calls=[]
 def spec(cpu):cpu.setg(3,SPEC+0x110+(cpu.g(4)&0xffffffff)*64)
 def wheelspec(cpu):cpu.setg(3,SPEC+0x50+(cpu.g(4)&0xffffffff)*48)
 def prepare(cpu):
  sensor=cpu.g(3)&0xffffffff;local=cpu.g(5)&0xffffffff;world=cpu.g(6)&0xffffffff;calls.append(('prepare',cpu.g(4)&0xffffffff,tuple(cpu.vr[i] for i in range(1,5))));cpu.mem.wr(sensor+0x19c,local.to_bytes(4,'big'));cpu.mem.wr(sensor+0x1a0,world.to_bytes(4,'big'));cpu.mem.wr(local,struct.pack('>4f',1.,2.,3.,.5));cpu.mem.wr(world,struct.pack('>4f',1.,2.,3.,.5));cpu.setg(3,1)
 def mapset(cpu):cpu.mem.wr((cpu.g(3)+4*(cpu.g(4)&0xffffffff))&0xffffffff,(cpu.g(5)&0xffffffff).to_bytes(4,'big'))
 def swept(cpu):cpu.mem.wr(cpu.g(3)&0xffffffff,struct.pack('>8I',*(cpu.vr[1]+cpu.vr[2])))
 c.hooks[0x8260a2e8]=prepare
 for k,v in b.unresolved.items():
  if 'GetDeformationSensorSpec' in v:c.hooks[k]=spec
  elif '?Prepare@DeformationSensor' in v:c.hooks[k]=prepare
  elif 'SetCollidableBodyMap' in v:c.hooks[k]=mapset
  elif 'GetWheelSpec' in v:c.hooks[k]=wheelspec
  elif '?Set@SweptSphere' in v:c.hooks[k]=swept
  elif v.startswith('__save') or v.startswith('__rest') or 'Assert' in v:c.hooks[k]=lambda cpu:None
 for i in range(1,5):c.vr[i]=struct.unpack('>4I',struct.pack('>4f',i*.1,i*.2,i*.3,i*.4))
 c.setg(3,A);c.run(b.entry if comp else 0x82623d60);return c.mem.rd(A,5*16).hex(),c.mem.rd(A+0x180,5*32).hex(),c.mem.rd(A+0x66e0,4).hex(),c.mem.rd(A+0x18e4,100).hex(),calls
for i in range(128):
 rnd=lambda n:struct.pack('>'+str(n)+'f',*[rng.uniform(-4,4) for _ in range(n)])
 transform=rnd(16);velocity=rnd(8);wheels=[rnd(4) for _ in range(4)];scales=[rnd(4) for _ in range(4)];init=rng.randbytes(0x7000)
 for enabled in (0,1):
  x=run(False,transform,velocity,wheels,scales,init,enabled);y=run(True,transform,velocity,wheels,scales,init,enabled)
  if x!=y:bad.append({'case':i,'enabled':enabled,'original':x,'compiled':y});break
 if bad:break
print('mismatches',len(bad),bad[:1]);(p/'lifecycle-sensors-comparison.json').write_text(json.dumps({'cases':256,'mismatches':bad,'scope':'whole ResetSensors 82623D60 vs actual object, 128 finite one-sensor/random wheel transforms and velocities with swept disabled/enabled; sensor Prepare, spec accessors, map and SweptSphere Set controlled, compares 5 world spheres, 5 sweeps, map and v1-v4 Prepare call; no actual service, nonfinite/hardware claim','object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest()},indent=2)+'\n');assert not bad
