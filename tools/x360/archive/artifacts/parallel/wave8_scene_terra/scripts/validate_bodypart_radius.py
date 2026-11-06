from pathlib import Path
import sys,json,random,hashlib,struct
sys.path.insert(0,r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\scratch\x360_sdk');import native_atomic_fixture as F
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/parallel/wave8_scene_terra/bodypart-bounded');obj=out/'bounded.obj';name='?GetSphereRadius@PhysicalBodyPart@Deformation@BrnPhysics@@';bn='?GetBoundingBox@PhysicalBodyPart@Deformation@BrnPhysics@@';b=F.Bundle([F.Coff(obj,name),F.Coff(obj,bn)]);b.select(name);target=F.address(b,bn);A=0x40000000;rng=random.Random(0x825E7E10);bad=[]
def run(comp,box):
 c=F.cpu(b);c.mem.add(A,0x1000);calls=[]
 def setbox(cpu):calls.append(cpu.g(3)&0xffffffff);cpu.mem.wr(cpu.g(4)&0xffffffff,box)
 c.hooks[0x825E7D28]=setbox;c.hooks[target]=setbox;c.setg(3,A);c.run(b.entry if comp else 0x825E7E10);return calls,struct.pack('>d',c.f(1)).hex()
for i in range(128):
 row=[rng.uniform(-100,100) for j in range(3)]+[rng.uniform(-10,10)];box=bytes(64)+struct.pack('>4f',*row);x=run(False,box);y=run(True,box)
 if x!=y:bad.append({'case':i,'original':x,'compiled':y})
r={'cases':128,'mismatches':bad,'scope':'actual original out-of-line GetSphereRadius825E7E10 vs bounded exact method body compiled with real types; controlled same GetBoundingBox output including arbitrary nonzero fatness W; finite random dimensions and fatness, compares receiver and f1 bits using VMX reciprocal-estimate emulator. No full provider TU, real bbox composition, nonfinite or hardware claim','object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest()};(out/'sphere-radius-comparison.json').write_text(json.dumps(r,indent=2)+'\n');print('cases',128,'mismatches',len(bad),bad[:1]);assert not bad
