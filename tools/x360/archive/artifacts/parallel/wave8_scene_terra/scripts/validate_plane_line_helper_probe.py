import sys,random,json,hashlib
from pathlib import Path
sys.path.insert(0,r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\scratch\x360_sdk');import native_atomic_fixture as F
sys.path.insert(0,r'D:\Reverse\BP-Decomp_Workflow\tools\re');import x360rd
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/parallel/wave8_scene_terra/volume-baseline');obj=out/'plane-line-helper-probe.obj';b=F.Bundle([F.Coff(obj,'?rwcPlaneLineSegIntersect@')]);b.select('?rwcPlaneLineSegIntersect@');rng=random.Random(0x82ba8818);O=0x50000000;bad=[];branches=set()
for i in range(256):
 args=[rng.uniform(-5,5) for _ in range(4)];initial=rng.randbytes(0x40);r=[]
 for compiled in (False,True):
  c=F.cpu(b);c.mem.add(O,0x40,initial)
  if not compiled:
   for a in [0x82001000,0x82180000]:c.mem.add(a,0x1000,x360rd.rd(a,0x1000))
  c.setg(3,O)
  for k,v in enumerate(args):c.setf(k+1,v)
  c.run(b.entry if compiled else 0x82ba8818);r.append((c.mem.rd(O,0x40),c.g(3)&0xffffffff))
 branches.add(r[0][1])
 if r[0]!=r[1]:bad.append([i,r[0][1],r[1][1],r[0][0][:8].hex(),r[1][0][:8].hex()])
print('cases 256 branches',branches,'mismatches',len(bad),bad[:2]);assert not bad and branches=={0,1,0xffffffff}
(out/'plane-line-helper-probe-comparison.json').write_text(json.dumps({'cases':256,'branches':sorted(branches),'mismatches':bad,'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'temporary extracted existing plane helper, finite all three return paths and Fraction guards, XDK fmsubs; NOT whole provider or denormal hardware claim'},indent=2))
