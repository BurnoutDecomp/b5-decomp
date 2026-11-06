import sys,random,json,struct,hashlib
from pathlib import Path
sys.path.insert(0,r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\scratch\x360_sdk');import native_atomic_fixture as F
sys.path.insert(0,r'D:\Reverse\BP-Decomp_Workflow\tools\re');import x360rd
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/parallel/wave8_scene_terra/volume-baseline');obj=out/'triangle-volume-methods-probe.obj';b=F.Bundle([F.Coff(obj,'?Initialize@TriangleVolume@')]);b.select('?Initialize@TriangleVolume@');rng=random.Random(0x82bb0680);A=0x50000000;R=0x50001000;bad=[]
for i in range(128):
 src=rng.randbytes(0x100);tab=rng.randbytes(0x1c);resource=bytearray(rng.randbytes(0x40));struct.pack_into('>I',resource,0,A if i%4 else 0);vectors=[tuple(rng.getrandbits(32) for _ in range(4)) for _ in range(3)];r=[]
 for compiled in (False,True):
  c=F.cpu(b);c.mem.add(A,0x100,src);c.mem.add(R,0x40,resource)
  if compiled:tbl=next(a for a,n in b.unresolved.items() if 'gVolumeVTable' in n)
  else:c.mem.add(0x8327eee0,0x40);c.mem.add(0x82001000,0x1000,x360rd.rd(0x82001000,0x1000));c.mem.add(0x82003000,0x1000,x360rd.rd(0x82003000,0x1000));tbl=0x8327eee0
  c.mem.wr(tbl,tab);c.setg(3,R)
  for k,v in enumerate(vectors):c.vr[k+1]=v
  c.run(b.entry if compiled else 0x82bb0680);r.append((c.mem.rd(A,0x100),c.mem.rd(R,0x40),c.g(3)&0xffffffff))
 if r[0]!=r[1]:bad.append([i,r[0][0].hex(),r[1][0].hex()])
print('cases 128 mismatches',len(bad),bad[:1]);assert not bad
(out/'triangle-volume-init-methods-probe-comparison.json').write_text(json.dumps({'cases':128,'mismatches':bad,'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'temporary extracted TriangleVolume initializer, null/non-null resource, raw vector bits incl NaNs, controlled descriptor table, entire object/guards; NOT whole provider'},indent=2))
