import sys,random,json,hashlib
from pathlib import Path
sys.path.insert(0,r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\scratch\x360_sdk');import native_atomic_fixture as F
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/parallel/wave8_scene_terra/volume-baseline');obj=out/'BoxVolume-final-wip.obj';b=F.Bundle([F.Coff(obj,'?GetMaximumFeature@SphereVolume@')]);rng=random.Random(0x82ba81b0);A=0x50000000;O=0x50001000;bad=[]
for typ,addr in [('SphereVolume',0x82ba81b0),('BoxVolume',0x82ba87f0)]:
 b.select('?GetMaximumFeature@'+typ+'@')
 for i in range(128):
  src=rng.randbytes(0x100);initial=rng.randbytes(0x300);r=[]
  for compiled in (False,True):
   c=F.cpu(b);c.mem.add(A,0x100,src);c.mem.add(O,0x300,initial);c.setg(3,A);c.setg(4,i&1)
   if typ=='SphereVolume':c.setg(5,A);c.setg(6,O)
   else:c.setg(5,O);c.vr[1]=tuple(rng.getrandbits(32) for _ in range(4))
   c.run(b.entry if compiled else addr);r.append((c.mem.rd(O,0x300),c.mem.rd(A,0x100),c.g(3)&0xffffffff))
  if r[0]!=r[1]:bad.append([typ,i])
print('cases 256 mismatches',len(bad),bad[:3]);assert not bad
(out/'maxfeature-whole-wip-final-comparison.json').write_text(json.dumps({'cases':256,'mismatches':bad,'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'actual whole BoxVolume.cpp object WIPs, raw XYZW incl NaN payloads, actual Sphere r6 versus Box r5 output ABI and full feature/guards; whole BoxVolume TU compiled, remaining descriptor closure open'},indent=2))

