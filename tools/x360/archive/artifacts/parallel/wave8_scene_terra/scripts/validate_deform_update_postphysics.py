from pathlib import Path
import sys,json,random,struct,hashlib
sys.path.insert(0,r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\scratch\x360_sdk');import native_atomic_fixture as F
class PostCPU(F.AtomicCPU):
 def exec_vector(self,a,w,mn):
  if w >> 26 == 5 and w & 0x3d0 == 0x250:
   vd=((w>>21)&31)|(((w>>2)&3)<<5);va=((w>>16)&31)|(((w>>5)&1)<<5)|(((w>>10)&1)<<6);vb=((w>>11)&31)|((w&3)<<5)
   self.vr[vd]=tuple(self.vr[va][i] & (~self.vr[vb][i]&0xffffffff) for i in range(4));return True
  return super().exec_vector(a,w,mn)
p=Path('D:/Reverse/BP-X360-Experimental-Artifacts/parallel/wave8_scene_terra/deformation-next');obj=p/'14.obj';n='?UpdatePostPhysics@DeformableObject@';o=F.Coff(obj,n);b=F.Bundle([o]);b.select(n);A=0x40000000;SPEC=A+0x9000;VEH=A+0xb000;rng=random.Random(0x825dfeb0);bad=[]
def run(comp,tr,vel,wheels,streams,scales,local,initial,enabled):
 c=F.cpu(b);c.__class__=PostCPU;c.mem.add(A,0x10000);c.mem.add(0x82f2a000,0x1000);c.mem.wr(A,initial);c.mem.wr(A+0x18e0,SPEC.to_bytes(4,'big'));c.mem.wr(A+0x194c,VEH.to_bytes(4,'big'));c.mem.wr(A+0x6722,b'\x01');c.mem.wr(A+0x672a,bytes([enabled]));c.mem.wr(SPEC+0x652,b'\x01');c.mem.wr(VEH+0x10,tr);c.mem.wr(VEH+0x50,vel);c.mem.wr(VEH+0x70,b'\x01');c.mem.wr(A+0xf60,local)
 for i in range(4):c.mem.wr(VEH+0x1b0+i*0xe0,wheels[i]);c.mem.wr(VEH+0x1c0+i*0xe0,streams[i]);c.mem.wr(SPEC+0x60+i*48,scales[i])
 def wheelspec(cpu):cpu.setg(3,SPEC+0x50+(cpu.g(4)&0xffffffff)*48)
 def swept(cpu):cpu.mem.wr(cpu.g(3)&0xffffffff,struct.pack('>8I',*(cpu.vr[1]+cpu.vr[2])))
 for k in (0x821f1198,0x821f1308,0x825c1050):c.hooks[k]=lambda cpu:None
 for k,v in b.unresolved.items():
  if 'GetWheelSpec' in v:c.hooks[k]=wheelspec
  elif '?Set@SweptSphere' in v:c.hooks[k]=swept
  elif v.startswith('__save') or v.startswith('__rest') or 'Assert' in v or 'ClearNonWorldContacts' in v:c.hooks[k]=lambda cpu:None
 c.setg(3,A);c.setg(4,A+0x8000);c.run(b.entry if comp else 0x825dfeb0);return c.mem.rd(A,5*16).hex(),c.mem.rd(A+0x180,5*32).hex(),c.mem.rd(A+0x1960,16).hex()
for i in range(128):
 rnd=lambda n:struct.pack('>'+str(n)+'f',*[rng.uniform(.25,4) for _ in range(n)])
 tr=rnd(16);vel=rnd(8);wheels=[rnd(4) for _ in range(4)];streams=[rnd(4) for _ in range(4)];scales=[rnd(4) for _ in range(4)];local=rnd(4);initial=rng.randbytes(0x7000)
 for enabled in (0,1):
  x=run(False,tr,vel,wheels,streams,scales,local,initial,enabled);y=run(True,tr,vel,wheels,streams,scales,local,initial,enabled)
  if x!=y:bad.append({'i':i,'enabled':enabled,'original':x,'compiled':y});break
 if bad:break
print('mismatches',len(bad),bad[:1]);(p/'update-postphysics-comparison.json').write_text(json.dumps({'cases':256,'mismatches':bad,'scope':'whole UpdatePostPhysics original vs actual object, active frozen vehicle, one sensor, 128 finite random transforms/wheels/velocities with sweeps off/on; monitor and sensor-clear/wheel-spec/SweptSphere services controlled, compares all world/sweep lanes and sensor displacement; assertions only passing path; no actual services, nonfinite or hardware','object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest()},indent=2)+'\n');assert not bad
