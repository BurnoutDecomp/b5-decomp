import sys,random,json,struct,hashlib
from pathlib import Path
sys.path.insert(0,r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\scratch\x360_sdk');import native_atomic_fixture as F
sys.path.insert(0,r'D:\Reverse\BP-Decomp_Workflow\tools\re');import x360rd
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/parallel/wave8_scene_terra/volume-baseline');obj=out/'VolumeBBoxQuery-bbox-wip.obj';b=F.Bundle([F.Coff(obj,'?GetOverlaps@VolumeBBoxQuery@')]);b.select('?GetOverlaps@VolumeBBoxQuery@');rng=random.Random(0x82bbbde8);A=0x50000000;V=0x50001000;D=0x50002000;G=0x50003000;GT=0x50004000;H=0x85000000;bad=[]
for i in range(128):
 query=bytearray(rng.randbytes(0x200));vol=bytearray(rng.randbytes(0x100));desc=bytearray(rng.randbytes(0x40));agg=bytearray(rng.randbytes(0x80));vt=bytearray(rng.randbytes(0x40));result=i&1
 for o,n in [(0,0),(4,0),(8,0),(12,0),(0x40,V),(0x44,0),(0xc0,0),(0xc4,0),(0xcc,0),(0xd8,0)]:struct.pack_into('>I',query,o,n)
 struct.pack_into('>f',vol,0,2.0);struct.pack_into('>I',vol,0x40,D);struct.pack_into('>I',vol,0x44,G);struct.pack_into('>I',desc,0,6);struct.pack_into('>I',agg,0x20,GT);struct.pack_into('>I',vt,0x18,H);r=[]
 for compiled in (False,True):
  c=F.cpu(b)
  for addr,img in [(A,query),(V,vol),(D,desc),(G,agg),(GT,vt)]:c.mem.add(addr,len(img),img)
  if not compiled:
   for z in [0x82001000,0x82180000]:c.mem.add(z,0x1000,x360rd.rd(z,0x1000))
  trace=[]
  def hook(cpu):trace.append(tuple(cpu.g(k)&0xffffffff for k in range(3,6)));cpu.setg(3,result)
  c.hooks[H]=hook;c.setg(3,A);c.run(b.entry if compiled else 0x82bbbde8);r.append((c.mem.rd(A,0x200),c.mem.rd(V,0x100),c.mem.rd(G,0x80),trace,c.g(3)&0xffffffff))
 if r[0]!=r[1]:bad.append([i,r[0][3],r[1][3],r[0][4],r[1][4]])
print('cases 128 mismatches',len(bad),bad[:2]);assert not bad
(out/'bbox-aggregate-dispatch-wip-comparison.json').write_text(json.dumps({'cases':128,'mismatches':bad,'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'actual whole GetOverlaps staged aggregate path, real Volume type/aggregate fields and Aggregate PMF slot +0x18, callback controlled success/failure, nonidentity transform, full poisoned query/volume/aggregate guards; no full spatial traversal claim'},indent=2))
