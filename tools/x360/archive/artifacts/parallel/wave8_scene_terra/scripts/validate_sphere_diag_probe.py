import sys,random,json,struct,hashlib
from pathlib import Path
sys.path.insert(0,r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\scratch\x360_sdk');import native_atomic_fixture as F
sys.path.insert(0,r'D:\Reverse\BP-Decomp_Workflow\tools\re');import x360rd
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/parallel/wave8_scene_terra/volume-baseline');obj=out/'sphere-diag-probe.obj';b=F.Bundle([F.Coff(obj,'?GetBBoxDiag@SphereVolume@')]);b.select('?GetBBoxDiag@SphereVolume@');rng=random.Random(0x82ba8580);A=0x50000000;O=0x50001000;bad=[]
for i in range(128):
 src=bytearray(rng.randbytes(0x100));struct.pack_into('>f',src,0x50,rng.uniform(-1e5,1e5));initial=rng.randbytes(0x100);res=[]
 for compiled in (False,True):
  c=F.cpu(b);c.mem.add(A,0x100,src);c.mem.add(O,0x100,initial)
  if not compiled:c.mem.add(0x82cda000,0x1000,x360rd.rd(0x82cda000,0x1000))
  c.setg(3,O);c.setg(4,A);c.run(b.entry if compiled else 0x82ba8580);res.append((c.mem.rd(O,0x100),c.mem.rd(A,0x100)))
 if res[0]!=res[1]:bad.append([i,res[0][0][:16].hex(),res[1][0][:16].hex()])
print('cases 128 mismatches',len(bad),bad[:3]);assert not bad
(out/'sphere-diag-probe-comparison.json').write_text(json.dumps({'cases':128,'mismatches':bad,'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'temporary extracted method probe, finite radii, full return lanes/guards; NOT whole BoxVolume TU or full provider'},indent=2))
