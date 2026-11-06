import sys,random,json,struct,hashlib
from pathlib import Path
sys.path.insert(0,r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\scratch\x360_sdk');import native_atomic_fixture as F
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/parallel/wave8_scene_terra/volume-baseline');obj=out/'VolumeQuery-enum-wip.obj';ref=out/'VolRef-typed-wip.obj';b=F.Bundle([F.Coff(obj,'?GetIntersections@VolumeLineQuery@'),F.Coff(ref,'??4VolRef@')]);b.select('?GetIntersections@VolumeLineQuery@');rng=random.Random(0x82bb3470);A=0x50000000;V=0x50001000;D=0x50002000;C=0x50003000;T=0x50004000;O=0x50005000;H=0x85000000;bad=[]
for i in range(128):
 query=bytearray(rng.randbytes(0x200));vol=bytearray(rng.randbytes(0x100));desc=bytearray(rng.randbytes(0x40));entry=bytearray(rng.randbytes(0x100));tm=rng.randbytes(0x80);output=rng.randbytes(0x100)
 for o,n in [(8,0),(12,0),(0x10,O),(0x18,1),(0x50,0),(0xd0,0),(0xd8,C),(0xdc,1)]:struct.pack_into('>I',query,o,n)
 struct.pack_into('>f',query,0x40,.25);struct.pack_into('>I',vol,0x40,D);struct.pack_into('>I',vol,0x5c,1);struct.pack_into('>I',desc,0,1);struct.pack_into('>I',desc,0x18,H);struct.pack_into('>I',entry,0,V);struct.pack_into('>I',entry,4,T if i&1 else 0);r=[]
 for compiled in (False,True):
  c=F.cpu(b)
  for addr,img in [(A,query),(V,vol),(D,desc),(C,entry),(T,tm),(O,output)]:c.mem.add(addr,len(img),img)
  trace=[]
  def hook(cpu):trace.append((tuple(cpu.g(k)&0xffffffff for k in range(3,8)),cpu.f(1)));cpu.setg(3,0)
  c.hooks[H]=hook;c.setg(3,A);c.run(b.entry if compiled else 0x82bb3470);r.append((c.mem.rd(A,0x200),c.mem.rd(V,0x100),c.mem.rd(C,0x100),c.mem.rd(O,0x100),trace,c.g(3)&0xffffffff))
 if r[0]!=r[1]:bad.append([i,r[0][4],r[1][4],r[0][5],r[1][5]])
print('cases 128 mismatches',len(bad),bad[:2]);assert not bad
(out/'line-query-primitive-wip-enum-comparison.json').write_text(json.dumps({'cases':128,'mismatches':bad,'query_object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'volref_object_sha256':hashlib.sha256(ref.read_bytes()).hexdigest(),'scope':'actual whole VolumeLineQuery staged primitive GetIntersections vs ARTIST 82BB3470; real +0x18 PMF dispatch, optional transform and controlled no-hit callee, r3-r7/f1 and poisoned buffers; does not certify Triangle slot ABI or full traversal'},indent=2))
