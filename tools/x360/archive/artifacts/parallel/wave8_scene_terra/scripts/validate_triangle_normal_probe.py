import sys,random,json,struct,hashlib
from pathlib import Path
sys.path.insert(0,r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\scratch\x360_sdk');import native_atomic_fixture as F
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/parallel/wave8_scene_terra/volume-baseline');obj=out/'triangle-volume-methods-probe.obj';b=F.Bundle([F.Coff(obj,'?GetNormal@TriangleVolume@')]);b.select('?GetNormal@TriangleVolume@');rng=random.Random(0x82bb0580);A=0x50000000;O=0x50001000;T=0x50002000;bad=[]
for i in range(128):
 src=bytearray(rng.randbytes(0x100));tm=bytearray(rng.randbytes(0x100));initial=rng.randbytes(0x100)
 for j in range(0,0x40,4):struct.pack_into('>f',src,j,rng.uniform(-10,10));struct.pack_into('>f',tm,j,rng.uniform(-3,3))
 if i%8==1:src[16:32]=src[:16]
 flags=struct.unpack_from('>I',src,0x5c)[0];struct.pack_into('>I',src,0x5c,(flags|2) if (i&1) else (flags&~2));r=[]
 for compiled in (False,True):
  c=F.cpu(b);c.mem.add(A,0x100,src);c.mem.add(O,0x100,initial);c.mem.add(T,0x100,tm)
  if compiled:tab=F.address(b,'VEC_EPSILON')
  else:c.mem.add(0x8327ef10,16);tab=0x8327ef10
  c.mem.wr(tab,bytes.fromhex('34000000')*4);c.setg(3,A);c.setg(4,O);c.setg(5,T if i&2 else 0);c.run(b.entry if compiled else 0x82bb0580);r.append((c.mem.rd(O,0x100),c.mem.rd(A,0x100),c.mem.rd(T,0x100)))
 if r[0]!=r[1]:bad.append([i,r[0][0][:16].hex(),r[1][0][:16].hex(),r[0][1][0x30:0x40].hex(),r[1][1][0x30:0x40].hex()])
print('cases 128 mismatches',len(bad),bad[:2]);assert not bad
(out/'triangle-volume-normal-probe-comparison.json').write_text(json.dumps({'cases':128,'mismatches':bad,'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'temporary extracted Triangle GetNormal, dirty/clean cache, optional transform and finite XYZ/W incl degeneracy, initialized epsilon controlled, full object/guards; estimate emulator, NOT whole provider'},indent=2))
