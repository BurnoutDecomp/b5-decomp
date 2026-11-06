import sys,random,json,struct,hashlib
from pathlib import Path
sys.path.insert(0,r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\scratch\x360_sdk');import native_atomic_fixture as F
sys.path.insert(0,r'D:\Reverse\BP-Decomp_Workflow\tools\re');import x360rd
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/parallel/wave8_scene_terra/volume-baseline');obj=out/'sphere-line-methods-probe.obj';b=F.Bundle([F.Coff(obj,'?LineSegIntersect@SphereVolume@')]);b.select('?LineSegIntersect@SphereVolume@');rng=random.Random(0x82ba82c8);A=0x50000000;O=0x50001000;T=0x50002000;P=0x50003000;bad=[];hits=0
for i in range(256):
 src=bytearray(rng.randbytes(0x100));tm=bytearray(rng.randbytes(0x100));initial=rng.randbytes(0x100)
 for j in range(0,0x40,4):struct.pack_into('>f',src,j,rng.uniform(-3,3));struct.pack_into('>f',tm,j,rng.uniform(-3,3))
 radius=rng.uniform(.1,2);struct.pack_into('>f',src,0x50,radius);points=bytearray(rng.randbytes(0x40));fat=rng.uniform(0,1)
 if i%4==0:
  struct.pack_into('>4f',src,0x30,0,0,0,0);struct.pack_into('>4f',points,0,-3,0,0,0);struct.pack_into('>4f',points,0x20,3,0,0,0)
 elif i%4==1:
  struct.pack_into('>4f',src,0x30,0,0,0,0);struct.pack_into('>4f',points,0,0,0,0,0);struct.pack_into('>4f',points,0x20,3,0,0,0)
 else:
  for j in list(range(0,16,4))+list(range(0x20,0x30,4)):struct.pack_into('>f',points,j,rng.uniform(-5,5))
 r=[]
 for compiled in (False,True):
  c=F.cpu(b);c.mem.add(A,0x100,src);c.mem.add(O,0x100,initial);c.mem.add(T,0x100,tm);c.mem.add(P,0x40,points)
  if not compiled:
   for a in [0x82001000,0x82180000]:c.mem.add(a,0x1000,x360rd.rd(a,0x1000))
  c.setg(3,A);c.setg(4,P);c.setg(5,P+0x20);c.setg(6,T if i%4==3 else 0);c.setg(7,O);c.setf(1,fat);c.run(b.entry if compiled else 0x82ba82c8);r.append((c.mem.rd(O,0x100),c.mem.rd(A,0x100),c.mem.rd(P,0x40),c.mem.rd(T,0x100),c.g(3)&0xffffffff))
 if r[0][4]:hits+=1
 if r[0]!=r[1]:bad.append([i,r[0][4],r[1][4],r[0][0][:0x50].hex(),r[1][0][:0x50].hex()])
print('cases 256 hits',hits,'mismatches',len(bad),bad[:2]);assert not bad and hits
(out/'sphere-line-methods-probe-comparison.json').write_text(json.dumps({'cases':256,'hits':hits,'mismatches':bad,'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'temporary extracted SphereVolume line and real compiled rwcSphereLineSegIntersect, finite targeted inside/outside/hit/miss, optional matrix, all object/result guards; emulator estimate and no whole LineSeg TU claim'},indent=2))
