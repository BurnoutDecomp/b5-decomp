import sys,random,json,struct,hashlib
from pathlib import Path
sys.path.insert(0,r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\scratch\x360_sdk');import native_atomic_fixture as F
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/parallel/wave8_scene_terra/volume-baseline');obj=out/'TriangleVolume-wip-v4.obj';b=F.Bundle([F.Coff(obj,'?GetPoints@TriangleVolume@')]);b.select('?GetPoints@TriangleVolume@');rng=random.Random(0x82bb11f8);A=0x50000000;O=0x50001000;T=0x50002000;bad=[]
for i in range(128):
 src=bytearray(rng.randbytes(0x100));tm=bytearray(rng.randbytes(0x100));initial=rng.randbytes(0x100)
 for j in range(0,0x40,4):struct.pack_into('>f',src,j,rng.uniform(-10,10));struct.pack_into('>f',tm,j,rng.uniform(-3,3))
 r=[]
 for compiled in (False,True):
  c=F.cpu(b);c.mem.add(A,0x100,src);c.mem.add(O,0x100,initial);c.mem.add(T,0x100,tm);c.setg(3,A);c.setg(4,O);c.setg(5,O+0x20);c.setg(6,O+0x40);c.setg(7,T if i&1 else 0);c.run(b.entry if compiled else 0x82bb11f8);r.append((c.mem.rd(O,0x100),c.mem.rd(A,0x100),c.mem.rd(T,0x100)))
 if r[0]!=r[1]:bad.append(i)
print('cases 128 mismatches',len(bad),bad[:3]);assert not bad
(out/'triangle-volume-points-v4-comparison.json').write_text(json.dumps({'cases':128,'mismatches':bad,'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'actual whole TriangleVolume CPP GetPoints, null/non-null transform, finite distinct full XYZ/W, nonoverlapping outputs, all object/guards; whole TriangleVolume compiles; TriangleLineSeg provider unresolved'},indent=2))
