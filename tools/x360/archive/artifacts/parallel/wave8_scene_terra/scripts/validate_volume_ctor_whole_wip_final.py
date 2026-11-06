import sys,random,json,struct,hashlib
from pathlib import Path
sys.path.insert(0,r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\scratch\x360_sdk');import native_atomic_fixture as F
sys.path.insert(0,r'D:\Reverse\BP-Decomp_Workflow\tools\re');import x360rd
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/parallel/wave8_scene_terra/volume-baseline');obj=out/'BoxVolume-final-wip.obj';b=F.Bundle([F.Coff(obj,'??0BoxVolume@')]);rng=random.Random(0x82baa0f0);A=0x50000000;R=0x50001000;bad=[]
for typ,prefix,addr in [('ctor','??0BoxVolume@',0x82baa0f0),('boxinit','?Initialize@BoxVolume@',0x82baa188),('sphereinit','?Initialize@SphereVolume@',0x82ba84e8)]:
 b.select(prefix)
 for i in range(128):
  src=rng.randbytes(0x100);tab=rng.randbytes(0x1c);resource=bytearray(rng.randbytes(0x40));ptr=A if (typ=='ctor' or i%4) else 0;struct.pack_into('>I',resource,0,ptr);radius=rng.uniform(-100,100);dims=tuple(struct.unpack('>I',struct.pack('>f',rng.uniform(-100,100)))[0] for _ in range(4));r=[]
  for compiled in (False,True):
   c=F.cpu(b);c.mem.add(A,0x100,src);c.mem.add(R,0x40,resource)
   if compiled:tbl=next(a for a,n in b.unresolved.items() if 'gVolumeVTable' in n)
   else:
    c.mem.add(0x8327eee0,0x40);c.mem.add(0x82181000,0x1000,x360rd.rd(0x82181000,0x1000));tbl=0x8327eee0
   c.mem.wr(tbl,tab);c.setg(3,A if typ=='ctor' else R);c.vr[1]=dims;c.setf(1,radius);c.run(b.entry if compiled else addr);r.append((c.mem.rd(A,0x100),c.mem.rd(R,0x40),c.g(3)&0xffffffff))
  if r[0]!=r[1]:bad.append([typ,i,r[0][0].hex(),r[1][0].hex(),r[0][2],r[1][2]])
print('cases 384 mismatches',len(bad),bad[:2]);assert not bad
(out/'volume-ctor-whole-wip-final-comparison.json').write_text(json.dumps({'cases':384,'mismatches':bad,'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'actual whole BoxVolume CPP: Box constructor and Box/Sphere Initialize, null/non-null resource, controlled descriptor table, finite vector/radius, entire object and guards'},indent=2))
