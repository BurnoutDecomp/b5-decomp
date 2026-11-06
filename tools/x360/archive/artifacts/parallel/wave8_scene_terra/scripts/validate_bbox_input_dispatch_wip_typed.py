import sys,random,json,struct,hashlib
from pathlib import Path
sys.path.insert(0,r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\scratch\x360_sdk');import native_atomic_fixture as F
sys.path.insert(0,r'D:\Reverse\BP-Decomp_Workflow\tools\re');import x360rd
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/parallel/wave8_scene_terra/volume-baseline');obj=out/'VolumeBBoxQuery-typed-wip.obj';b=F.Bundle([F.Coff(obj,'?GetOverlaps@VolumeBBoxQuery@')]);b.select('?GetOverlaps@VolumeBBoxQuery@');rng=random.Random(0x82bbbde8);A=0x50000000;V=0x50001000;D=0x50002000;I=0x50003000;C=0x50004000;H=0x85000000;bad=[]
for i in range(128):
 query=bytearray(rng.randbytes(0x200));vol=bytearray(rng.randbytes(0x100));desc=bytearray(rng.randbytes(0x40));inputs=bytearray(rng.randbytes(0x40));prims=rng.randbytes(0x200);overlap=bool(i&1)
 for o,n in [(0,I),(4,0),(8,1),(12,0),(0x40,0),(0xc0,0),(0xc4,0),(0xc8,C),(0xcc,0),(0xd0,1),(0xd8,0)]:struct.pack_into('>I',query,o,n)
 query[0x10:0x30]=struct.pack('>8f',-1,-1,-1,17,1,1,1,-13);struct.pack_into('>I',inputs,0,V);struct.pack_into('>I',vol,0x40,D);struct.pack_into('>I',vol,0x5c,1);struct.pack_into('>I',desc,0,1);struct.pack_into('>I',desc,4,H);bbox=struct.pack('>8f',*(-.5,-.5,-.5,12,.5,.5,.5,-7) if overlap else (4,4,4,12,5,5,5,-7));r=[]
 for compiled in (False,True):
  c=F.cpu(b)
  for addr,img in [(A,query),(V,vol),(D,desc),(I,inputs),(C,prims)]:c.mem.add(addr,len(img),img)
  if not compiled:
   for z in [0x82001000,0x82180000]:c.mem.add(z,0x1000,x360rd.rd(z,0x1000))
  trace=[]
  def hook(cpu):
   trace.append(tuple(cpu.g(k)&0xffffffff for k in range(3,6)));cpu.mem.wr(cpu.g(6)&0xffffffff,bbox);cpu.setg(3,1)
  c.hooks[H]=hook;c.setg(3,A);c.run(b.entry if compiled else 0x82bbbde8);r.append((c.mem.rd(A,0x200),c.mem.rd(V,0x100),c.mem.rd(C,0x200),trace,c.g(3)&0xffffffff))
 if r[0]!=r[1]:bad.append([i,r[0][3],r[1][3],r[0][4],r[1][4]])
print('cases 128 mismatches',len(bad),bad[:2]);assert not bad
(out/'bbox-input-dispatch-typed-wip-comparison.json').write_text(json.dumps({'cases':128,'mismatches':bad,'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'actual whole GetOverlaps input primitive path, direct real Volume GetBBox PMF slot +4, controlled bbox overlapping/separated, full poisoned query/volume/result guards; no aggregate traversal claim'},indent=2))
