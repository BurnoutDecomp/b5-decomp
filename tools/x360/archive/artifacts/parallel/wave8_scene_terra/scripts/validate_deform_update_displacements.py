from pathlib import Path
import sys,json,random,struct,hashlib
sys.path.insert(0,r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\scratch\x360_sdk');import native_atomic_fixture as F
p=Path('D:/Reverse/BP-X360-Experimental-Artifacts/parallel/wave8_scene_terra/deformation-next');obj=p/'14.obj';n='?UpdateSensorDisplacements@DeformableObject@';o=F.Coff(obj,n);b=F.Bundle([o]);b.select(n);A=0x40000000;SPEC=A+0x9000;VEH=A+0xb000;SP=A+0xd000;rng=random.Random(0x825df898);bad=[]
def run(comp,transform,velocity,spheres,disps,dt):
 c=F.cpu(b);c.mem.add(A,0x10000);c.mem.wr(A+0x18e0,SPEC.to_bytes(4,'big'));c.mem.wr(A+0x194c,VEH.to_bytes(4,'big'));c.mem.wr(A+0x6722,b'\x01');c.mem.wr(SPEC+0x652,bytes([len(spheres)]));c.mem.wr(VEH+0x10,transform);c.mem.wr(VEH+0x50,velocity)
 for i,sp in enumerate(spheres):c.mem.wr(A+0x1950+i*0x1b0+0x19c,(SP+i*16).to_bytes(4,'big'));c.mem.wr(A+0x1950+i*0x1b0+0x10,disps[i]);c.mem.wr(SP+i*16,sp)
 for k,v in b.unresolved.items():
  if 'Assert' in v or v.startswith('__save') or v.startswith('__rest'):c.hooks[k]=lambda cpu:None
 c.setg(3,A);c.vr[1]=struct.unpack('>4I',dt);c.run(b.entry if comp else 0x825df898);return [c.mem.rd(A+0x1950+i*0x1b0+0x10,16).hex() for i in range(len(spheres))]
for i in range(128):
 rnd=lambda n:struct.pack('>'+str(n)+'f',*[rng.uniform(-4,4) for _ in range(n)])
 tr=rnd(16);vel=rnd(8);sps=[rnd(4) for j in range(i%5)];ds=[rnd(4) for j in sps];dt=rnd(4);x=run(False,tr,vel,sps,ds,dt);y=run(True,tr,vel,sps,ds,dt)
 if x!=y:bad.append({'case':i,'original':x,'compiled':y});break
print('mismatches',len(bad),bad[:1]);(p/'update-displacements-comparison.json').write_text(json.dumps({'cases':128,'mismatches':bad,'scope':'whole UpdateSensorDisplacements 825DF898 vs actual object, 0-4 sensors, finite distinct timestep/transform/velocity/xyzW and poisoned displacement W, active=true; full displacement bytes, no invalid pointers/nonfinite/hardware claim','object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest()},indent=2)+'\n');assert not bad
