from pathlib import Path
import sys,json,random,hashlib
sys.path.insert(0,r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\scratch\x360_sdk');import native_atomic_fixture as F
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/parallel/wave8_scene_terra/deformable-header');obj=out/'probe.obj';name='?ProbeRadius@@';b=F.Bundle([F.Coff(obj,name)]);b.select(name);A=0x40000000;rng=random.Random(0x825BD1F8);bad=[]
for i in range(256):
 x=rng.randbytes(16);c=F.cpu(b);c.mem.add(A,0x10000);c.mem.wr(A+0x66d0,x);c.setg(3,A);c.run(b.entry);v=b''.join(z.to_bytes(4,'big') for z in c.vr[1]);d=F.cpu(b);d.mem.add(A,0x1000);d.mem.wr(A,x);d.setg(3,A+0x100);d.setg(4,A);d.run(0x825BD1F8);expect=d.mem.rd(A+0x100,16)
 if v!=expect:bad.append(i)
r={'cases':256,'mismatches':bad,'scope':'actual compiled GetEntitySphereSize inline raw W splat at measured member+0x66d0 vs original Sphere GetRadius825BD1F8 identical vspltw3 operation for arbitrary raw lane bits incl NaNs; not a whole deformable object layout/behavior claim','object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest()};(out/'comparison.json').write_text(json.dumps(r,indent=2)+'\n');print('cases',256,'mismatches',len(bad));assert not bad
