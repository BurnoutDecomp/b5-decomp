import sys,random,json,struct,hashlib
from pathlib import Path
sys.path.insert(0,r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\scratch\x360_sdk');import native_atomic_fixture as F
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/parallel/wave8_scene_terra/volume-baseline');obj=out/'box-wip7.obj';b=F.Bundle([F.Coff(obj,'?GetBBoxDiag@BoxVolume@')]);b.select('?GetBBoxDiag@BoxVolume@');rng=random.Random(0x82ba8890);A=0x50000000;O=0x50001000;bad=[]
for i in range(128):
 src=bytearray(rng.randbytes(0x100))
 for j in list(range(0,0x40,4))+[0x44,0x48,0x4c,0x50]:struct.pack_into('>f',src,j,rng.uniform(-100,100))
 initial=rng.randbytes(0x100);res=[]
 for compiled in (False,True):
  c=F.cpu(b);c.mem.add(A,0x100,src);c.mem.add(O,0x100,initial);c.setg(3,O);c.setg(4,A);c.run(b.entry if compiled else 0x82ba8890);res.append((c.mem.rd(O,0x100),c.mem.rd(A,0x100)))
 if res[0]!=res[1]:bad.append([i,res[0][0][:16].hex(),res[1][0][:16].hex()])
print('cases 128 mismatches',len(bad),bad[:3]);assert not bad
(out/'box-diag-whole-wip-comparison.json').write_text(json.dumps({'cases':128,'mismatches':bad,'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'actual whole BoxVolume.cpp object WIP, finite distinct XYZW, full return lanes/guards; whole BoxVolume TU compiled, remaining descriptor closure open or full provider'},indent=2))
