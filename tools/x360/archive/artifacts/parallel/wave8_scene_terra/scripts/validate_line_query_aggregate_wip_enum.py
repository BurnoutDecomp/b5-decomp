import sys,random,json,struct,hashlib
from pathlib import Path
sys.path.insert(0,r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\scratch\x360_sdk');import native_atomic_fixture as F
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/parallel/wave8_scene_terra/volume-baseline');obj=out/'VolumeQuery-enum-wip.obj';ref=out/'VolRef-typed-wip.obj';b=F.Bundle([F.Coff(obj,'?GetIntersections@VolumeLineQuery@'),F.Coff(ref,'??4VolRef@')]);b.select('?GetIntersections@VolumeLineQuery@');rng=random.Random(0x82bb3610);A=0x50000000;V=0x50001000;D=0x50002000;G=0x50003000;GT=0x50004000;T=0x50005000;H=0x85000000;bad=[]
for i in range(128):
 query=bytearray(rng.randbytes(0x200));vol=bytearray(rng.randbytes(0x100));desc=bytearray(rng.randbytes(0x40));agg=bytearray(rng.randbytes(0x80));vt=bytearray(rng.randbytes(0x40));tm=bytearray(rng.randbytes(0x80));result=i&1
 for j in range(0,0x40,4):struct.pack_into('>f',vol,j,rng.uniform(-3,3));struct.pack_into('>f',tm,j,rng.uniform(-3,3))
 for o,n in [(8,0),(12,0),(0x18,1),(0x50,V),(0x54,T if i&2 else 0),(0xd0,0),(0xdc,0)]:struct.pack_into('>I',query,o,n)
 struct.pack_into('>I',vol,0x40,D);struct.pack_into('>I',vol,0x44,G);struct.pack_into('>I',desc,0,6);struct.pack_into('>I',agg,0x20,GT);struct.pack_into('>I',vt,0x14,H);r=[]
 for compiled in (False,True):
  c=F.cpu(b)
  for addr,img in [(A,query),(V,vol),(D,desc),(G,agg),(GT,vt),(T,tm)]:c.mem.add(addr,len(img),img)
  trace=[]
  def hook(cpu):
   ptr=cpu.g(5)&0xffffffff;trace.append((cpu.g(3)&0xffffffff,cpu.g(4)&0xffffffff,cpu.mem.rd(ptr,64).hex()));cpu.setg(3,1 if result or len(trace)>1 else 0)
  c.hooks[H]=hook;c.setg(3,A);c.run(b.entry if compiled else 0x82bb3470);r.append((c.mem.rd(A,0x200),c.mem.rd(V,0x100),c.mem.rd(G,0x80),c.mem.rd(T,0x80),trace,c.g(3)&0xffffffff))
 if r[0]!=r[1]:bad.append([i,r[0][4],r[1][4]])
print('cases 128 mismatches',len(bad),bad[:1]);assert not bad
(out/'line-query-aggregate-wip-enum-comparison.json').write_text(json.dumps({'cases':128,'mismatches':bad,'query_object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'volref_object_sha256':hashlib.sha256(ref.read_bytes()).hexdigest(),'scope':'actual whole VolumeLineQuery staged aggregate path vs ARTIST 82BB3470; true Aggregate PMF slot +0x14, callback controlled immediate or one retry then success, parent transform null/finite full lanes, full poisoned buffers; no aggregate provider traversal claim'},indent=2))
