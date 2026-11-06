import sys,random,json,struct,hashlib
from pathlib import Path
sys.path.insert(0,r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\scratch\x360_sdk');import native_atomic_fixture as F
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/parallel/wave8_scene_terra/volume-baseline');obj=out/'BoxVolume-wip-v4.obj';b=F.Bundle([F.Coff(obj,'?GetBBox@SphereVolume@')]);rng=random.Random(0x82ba8020);A=0x50000000;O=0x50001000;T=0x50002000;bad=[]
for typ,addr in [('SphereVolume',0x82ba8020),('BoxVolume',0x82ba9fc8)]:
 b.select('?GetBBox@'+typ+'@')
 for i in range(128):
  src=bytearray(rng.randbytes(0x100));tm=bytearray(rng.randbytes(0x100));initial=rng.randbytes(0x100)
  for j in list(range(0,0x40,4))+[0x44,0x48,0x4c,0x50]:struct.pack_into('>f',src,j,rng.uniform(-100,100))
  for j in range(0,0x40,4):struct.pack_into('>f',tm,j,rng.uniform(-5,5))
  r=[]
  for compiled in (False,True):
   c=F.cpu(b);c.mem.add(A,0x100,src);c.mem.add(O,0x100,initial);c.mem.add(T,0x100,tm);c.setg(3,A);c.setg(4,T if i&1 else 0);c.setg(5,i&1);c.setg(6,O);c.run(b.entry if compiled else addr);r.append((c.mem.rd(O,0x100),c.mem.rd(A,0x100),c.mem.rd(T,0x100),c.g(3)&0xffffffff))
  if r[0]!=r[1]:bad.append([typ,i,r[0][0][:32].hex(),r[1][0][:32].hex()])
print('cases 256 mismatches',len(bad),bad[:3]);assert not bad
(out/'bbox-whole-wip-v4-comparison.json').write_text(json.dumps({'cases':256,'mismatches':bad,'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'actual whole BoxVolume.cpp object WIPs, finite distinct XYZW, both null/non-null transforms, full AABB and guards; whole BoxVolume TU compiled, remaining descriptor closure open'},indent=2))
