import sys,random,json,struct,hashlib
from pathlib import Path
sys.path.insert(0,r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\scratch\x360_sdk');import native_atomic_fixture as F
sys.path.insert(0,r'D:\Reverse\BP-Decomp_Workflow\tools\re');import x360rd
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/parallel/wave8_scene_terra/volume-baseline');obj=out/'CylinderVolume-wip-v4.obj';b=F.Bundle([F.Coff(obj,'?GetBBox@CylinderVolume@')]);b.select('?GetBBox@CylinderVolume@');rng=random.Random(0x82bad490);A=0x50000000;O=0x50001000;T=0x50002000;bad=[]
for i in range(192):
 src=bytearray(rng.randbytes(0x100));tm=bytearray(rng.randbytes(0x100));initial=rng.randbytes(0x100)
 for j in [0x44,0x48,0x50]:struct.pack_into('>f',src,j,rng.uniform(-100,100))
 for j in range(0,0x40,4):struct.pack_into('>f',tm,j,rng.uniform(-5,5))
 r=[]
 for compiled in (False,True):
  c=F.cpu(b);c.mem.add(A,0x100,src);c.mem.add(O,0x100,initial);c.mem.add(T,0x100,tm)
  if not compiled:c.mem.add(0x82001000,0x1000,x360rd.rd(0x82001000,0x1000))
  c.setg(3,A);c.setg(4,0 if i%3==0 else T);c.setg(5,1 if i%3==2 else 0);c.setg(6,O);c.run(b.entry if compiled else 0x82bad490);r.append((c.mem.rd(O,0x100),c.mem.rd(A,0x100),c.mem.rd(T,0x100),c.g(3)&0xffffffff))
 if r[0]!=r[1]:bad.append([i,r[0][0][:32].hex(),r[1][0][:32].hex()])
print('cases 192 mismatches',len(bad),bad[:3]);assert not bad
(out/'cylinder-bbox-whole-wip-v4-comparison.json').write_text(json.dumps({'cases':192,'mismatches':bad,'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'actual whole Cylinder bbox provider; finite distinct lanes and all three branches, full AABB/guards; estimate emulator, whole Cylinder compiled; descriptor closure remains open/hardware'},indent=2))
