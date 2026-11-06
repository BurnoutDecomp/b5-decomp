import sys,random,json,struct,hashlib
from pathlib import Path
sys.path.insert(0,r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\scratch\x360_sdk');import native_atomic_fixture as F
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/parallel/wave8_scene_terra/volume-baseline');obj=out/'ClusteredMeshQuery-addresult-wip.obj';tri=out/'TriangleVolume-enum-wip.obj';b=F.Bundle([F.Coff(obj,'?AddQueryResult@collision@rw@@'),F.Coff(tri,'?GetNormal@TriangleVolume@')]);b.select('?AddQueryResult@collision@rw@@');rng=random.Random(0x82bb1588);A=0x50000000;O=0x50001000;V=0x50002000;T=0x50003000;I=0x50004000;P=0x50005000;bad=[]
for i in range(128):
 query=bytearray(rng.randbytes(0x200));outbuf=rng.randbytes(0x100);vol=bytearray(rng.randbytes(0x100));tm=bytearray(rng.randbytes(0x100));inputs=bytearray(rng.randbytes(0x40));tags=bytearray(rng.randbytes(0x80));
 for j in range(0,0x40,4):struct.pack_into('>f',vol,j,rng.uniform(-10,10));struct.pack_into('>f',tm,j,rng.uniform(-3,3))
 flags=struct.unpack_from('>I',vol,0x5c)[0];struct.pack_into('>I',vol,0x5c,(flags|2) if i&1 else (flags&~2))
 for o,n in [(0,I),(0xc,1),(0x14,1 if i&2 else 0),(0x18,1),(0xe8,1 if i&4 else 0),(0xec,1),(0x104,3)]:struct.pack_into('>I',query,o,n)
 query[0x108]=i%8;struct.pack_into('>I',inputs,0,V);struct.pack_into('>I',tags,0,rng.randrange(0,64));struct.pack_into('>I',tags,0x10,rng.randrange(0,1024));tags[0x20]=i%32;struct.pack_into('>I',tags,0x30,6+i%7);struct.pack_into('>I',tags,0x40,i%4)
 vectors=[tuple(struct.unpack('>I',struct.pack('>f',rng.uniform(-5,5)))[0] for _ in range(4)) for k in range(4)];r=[]
 for compiled in (False,True):
  c=F.cpu(b)
  for addr,img in [(A,query),(O,outbuf),(V,vol),(T,tm),(I,inputs),(P,tags)]:c.mem.add(addr,len(img),img)
  if compiled:tab=F.address(b,'VEC_EPSILON')
  else:c.mem.add(0x8327ef10,16);tab=0x8327ef10
  c.mem.wr(tab,bytes.fromhex('34000000')*4)
  for k,x in enumerate([A,O,V,T,P,P+0x10,P+0x20,P+0x30]):c.setg(k+3,x)
  F.put(c,(c.g(1)+0x94)&0xffffffff,P+0x40)
  for k,v in enumerate(vectors):c.vr[k+1]=v
  c.run(b.entry if compiled else 0x82bb1588);r.append((c.mem.rd(A,0x200),c.mem.rd(O,0x100),c.mem.rd(V,0x100),c.mem.rd(T,0x100),c.g(3)&0xffffffff))
 if r[0]!=r[1]:bad.append([i,r[0][1][:0xd0].hex(),r[1][1][:0xd0].hex(),r[0][4],r[1][4]])
print('cases 128 mismatches',len(bad),bad[:1]);assert not bad
(out/'add-query-result-wip-comparison.json').write_text(json.dumps({'cases':128,'mismatches':bad,'query_object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'triangle_object_sha256':hashlib.sha256(tri.read_bytes()).hexdigest(),'scope':'actual whole ClusteredMeshQuery AddQueryResult linked TriangleVolume::GetNormal vs ARTIST 82BB1588, finite distinct v1-v4, dirty/clean normal, valid tag shifts, capacity equality branches, full poisoned query/result/volume/transform guards; threshold initialized, no full ClusteredMesh traversal or unique type-spelling claim'},indent=2))
