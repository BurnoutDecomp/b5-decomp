import sys,random,json,struct,hashlib
from pathlib import Path
sys.path.insert(0,r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\scratch\x360_sdk');import native_atomic_fixture as F
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/parallel/wave8_scene_terra/volume-baseline');obj=out/'VolumeBBoxQuery-typed-wip.obj';b=F.Bundle([F.Coff(obj,'?AddVolumeRef@VolumeBBoxQuery@')]);b.select('?AddVolumeRef@VolumeBBoxQuery@');rng=random.Random(0x82bbbc20);A=0x50000000;V=0x50001000;D=0x50002000;B=0x50003000;C=0x50004000;T=0x50005000;X=0x50006000;bad=[]
for i in range(128):
 query=bytearray(rng.randbytes(0x200));vol=bytearray(rng.randbytes(0x100));desc=bytearray(rng.randbytes(0x40));stack=rng.randbytes(0x200);prims=rng.randbytes(0x200);tm=rng.randbytes(0x80);bb=rng.randbytes(0x40);typ=6 if i&1 else 1;full=(i>>1)&1;tag=rng.getrandbits(32);nb=i%32
 struct.pack_into('>I',vol,0x40,D);struct.pack_into('>I',desc,0,typ);struct.pack_into('>I',query,0x30,B);struct.pack_into('>I',query,0xc0,0);struct.pack_into('>I',query,0xc4,0 if full else 1);struct.pack_into('>I',query,0xc8,C);struct.pack_into('>I',query,0xcc,0);struct.pack_into('>I',query,0xd0,0 if full else 1);r=[]
 for compiled in (False,True):
  c=F.cpu(b)
  for addr,img in [(A,query),(V,vol),(D,desc),(B,stack),(C,prims),(T,tm),(X,bb)]:c.mem.add(addr,len(img),img)
  c.setg(3,A);c.setg(4,V);c.setg(5,T if i&4 else 0);c.setg(6,X);c.setg(7,tag);c.setg(8,nb);c.run(b.entry if compiled else 0x82bbbc20);r.append(tuple(c.mem.rd(addr,n) for addr,n in [(A,0x200),(V,0x100),(B,0x200),(C,0x200),(T,0x80),(X,0x40)])+(c.g(3)&0xffffffff,))
 if r[0]!=r[1]:bad.append([i,r[0][-1],r[1][-1]])
print('cases 128 mismatches',len(bad),bad[:3]);assert not bad
(out/'bbox-addvol-typed-wip-comparison.json').write_text(json.dumps({'cases':128,'mismatches':bad,'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'actual whole VolumeBBoxQuery::AddVolumeRef with real compiled AddPrimitiveRef vs ARTIST 82BBBC20; descriptor type 1/6, full/available buffers, optional transform, full poisoned query/ref/bbox buffers; no aggregate callbacks'},indent=2))
