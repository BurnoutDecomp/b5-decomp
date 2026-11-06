import sys,random,json,struct,hashlib
from pathlib import Path
sys.path.insert(0,r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\scratch\x360_sdk');import native_atomic_fixture as F
sys.path.insert(0,r'D:\Reverse\BP-Decomp_Workflow\tools\re');import x360rd
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/parallel/wave8_scene_terra/volume-baseline');obj=out/'cylinder-gp-probe.obj';b=F.Bundle([F.Coff(obj,'?CreateGPInstance@CylinderVolume@')]);rng=random.Random(0x82ba92e8);A=0x50000000;O=0x50001000;T=0x50002000;bad=[]
for typ,addr in [('CylinderVolume',0x82bac7f8)]:
 b.select('?CreateGPInstance@'+typ+'@')
 for i in range(128):
  src=bytearray(rng.randbytes(0x100));tm=bytearray(rng.randbytes(0x100));initial=rng.randbytes(0x200);table=rng.randbytes(0x60)
  for j in list(range(0,0x40,4))+[0x44,0x48,0x4c,0x50]:struct.pack_into('>f',src,j,rng.uniform(-100,100))
  for j in range(0,0x40,4):struct.pack_into('>f',tm,j,rng.uniform(-5,5))
  r=[]
  for compiled in (False,True):
   c=F.cpu(b);c.mem.add(A,0x100,src);c.mem.add(O,0x200,initial);c.mem.add(T,0x100,tm)
   if compiled:tbl=next(a for a,n in b.unresolved.items() if 'g_aGPVolumeMethods' in n)
   else:
    c.mem.add(0x82f91800,0x200);c.mem.add(0x82cda000,0x1000,x360rd.rd(0x82cda000,0x1000));tbl=0x82f918f0
   c.mem.wr(tbl,table);c.setg(3,A);c.setg(4,O);c.setg(5,T if i&1 else 0);c.run(b.entry if compiled else addr);r.append((c.mem.rd(O,0x200),c.mem.rd(A,0x100),c.mem.rd(T,0x100),c.g(3)&0xffffffff))
  if r[0]!=r[1]:bad.append([typ,i,r[0][0][:0xc0].hex(),r[1][0][:0xc0].hex()])
print('cases 128 mismatches',len(bad),bad[:2]);assert not bad
(out/'cylinder-gp-probe-comparison.json').write_text(json.dumps({'cases':128,'mismatches':bad,'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'temporary extracted CylinderVolume method probe, finite distinct XYZW, both null/non-null transforms, controlled same four-pointer GP method table, full GP and guards; no downstream dispatch'},indent=2))
