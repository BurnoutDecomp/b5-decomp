import sys,random,json,struct,hashlib
from pathlib import Path
sys.path.insert(0,r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\scratch\x360_sdk');import native_atomic_fixture as F
sys.path.insert(0,r'D:\Reverse\BP-Decomp_Workflow\tools\re');import x360rd
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/parallel/wave8_scene_terra/volume-baseline');obj=out/'TriangleVolume-wip-v4.obj';b=F.Bundle([F.Coff(obj,'?CreateGPInstance@TriangleVolume@')]);b.select('?CreateGPInstance@TriangleVolume@');rng=random.Random(0x82bbaa00);A=0x50000000;O=0x50001000;T=0x50002000;bad=[]
for i in range(128):
 src=bytearray(rng.randbytes(0x100));tm=bytearray(rng.randbytes(0x100));initial=rng.randbytes(0x200);table=rng.randbytes(0x60)
 for j in list(range(0,0x40,4))+[0x44,0x48,0x4c,0x50]:struct.pack_into('>f',src,j,rng.uniform(-10,10))
 for j in range(0,0x40,4):struct.pack_into('>f',tm,j,rng.uniform(-3,3))
 if i%8==1:src[16:32]=src[:16]
 flags=struct.unpack_from('>I',src,0x5c)[0];struct.pack_into('>I',src,0x5c,(flags|2) if (i&1) else (flags&~2));r=[]
 for compiled in (False,True):
  c=F.cpu(b);c.mem.add(A,0x100,src);c.mem.add(O,0x200,initial);c.mem.add(T,0x100,tm)
  if compiled:
   tbl=next(a for a,n in b.unresolved.items() if 'g_aGPVolumeMethods' in n);eps=F.address(b,'VEC_EPSILON')
  else:
   c.mem.add(0x82f91800,0x200);c.mem.add(0x82cda000,0x1000,x360rd.rd(0x82cda000,0x1000));c.mem.add(0x82181000,0x1000,x360rd.rd(0x82181000,0x1000));c.mem.add(0x8327ef10,16);tbl=0x82f918f0;eps=0x8327ef10
  c.mem.wr(tbl,table);c.mem.wr(eps,bytes.fromhex('34000000')*4);c.setg(3,A);c.setg(4,O);c.setg(5,T if i&2 else 0);c.run(b.entry if compiled else 0x82bbaa00);r.append((c.mem.rd(O,0x200),c.mem.rd(A,0x100),c.mem.rd(T,0x100),c.g(3)&0xffffffff))
 if r[0]!=r[1]:bad.append([i,r[0][0][:0xc0].hex(),r[1][0][:0xc0].hex()])
print('cases 128 mismatches',len(bad),bad[:2]);assert not bad
(out/'triangle-gp-v4-comparison.json').write_text(json.dumps({'cases':128,'mismatches':bad,'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'actual whole TriangleVolume CPP GP and actual GetNormal methods; finite/nondegenerate and degenerate, cached/dirty, optional matrix, controlled GP table and initialized epsilon; full buffers/guards, emulator estimates; whole TriangleVolume compiles; TriangleLineSeg provider unresolved'},indent=2))
