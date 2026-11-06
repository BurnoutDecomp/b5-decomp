from pathlib import Path
import sys,json,random,hashlib,struct
sys.path.insert(0,r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\scratch\x360_sdk');import native_atomic_fixture as F
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/parallel/wave8_scene_terra/bodypart-baseline');obj=out/'bodypart.obj';n='?CalculateAABBExtents@PhysicalBodyPart@Deformation@BrnPhysics@@';b=F.Bundle([F.Coff(obj,n)]);b.select(n);A=0x40000000;rng=random.Random(0x825E2EA0);bad=[]
def run(comp,orient,half):
 c=F.cpu(b);c.mem.add(A,0x2000);c.mem.wr(A+0x1dc,(A+0x400).to_bytes(4,'big'));c.mem.wr(A+0x408,(A+0x600).to_bytes(4,'big'));c.mem.wr(A+0x640,orient);c.mem.wr(A+0x1a0,half);c.mem.wr(A+0x900,b'\xA5'*48);c.setg(3,A+0x910);c.setg(4,A);c.run(b.entry if comp else 0x825E2EA0);return c.mem.rd(A+0x900,48).hex()
for i in range(128):
 orient=struct.pack('>16f',*[rng.uniform(-10,10) for j in range(16)]);half=struct.pack('>4f',*[rng.uniform(-10,10) for j in range(4)]);a=run(False,orient,half);z=run(True,orient,half)
 if a!=z:bad.append({'case':i,'original':a,'compiled':z})
r={'cases':128,'mismatches':bad,'scope':'whole original CalculateAABBExtents825E2EA0 vs exact bounded actual source method/real headers, finite random orientation/half xyzw; compares full output16 incl W and guards, hidden sret r3/this r4. No full owning TU, arbitrary exceptional floats or hardware claim','object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest()};(out/'aabb-whole-comparison.json').write_text(json.dumps(r,indent=2)+'\n');print('cases',128,'mismatches',len(bad),bad[:1]);assert not bad
