from pathlib import Path
import sys,json,random,hashlib
sys.path.insert(0,r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\scratch\x360_sdk');import native_atomic_fixture as F
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/parallel/wave8_scene_terra/sphere13-line-abi');obj=out/'0.obj';name='?TestAxisAlignedBoxAxisAlignedBox@CgsGeometric@@';b=F.Bundle([F.Coff(obj,name)]);b.select(name);A=0x40000000;rng=random.Random(0x82812460);bad=[]
def run(comp,a,z):
 c=F.cpu(b);c.mem.add(A,0x1000);c.mem.wr(A,a);c.mem.wr(A+0x100,z);c.setg(3,A);c.setg(4,A+0x100);c.run(b.entry if comp else 0x82812460);return list(c.vr[1])
for i in range(512):
 a=rng.randbytes(32);z=rng.randbytes(32);x=run(False,a,z);y=run(True,a,z)
 if x!=y:bad.append({'case':i,'original':x,'compiled':y})
r={'cases':512,'mismatches':bad,'scope':'whole original 82812460 vs compiled MaskScalar AABB overlap; arbitrary raw bits in both min/max xyz/W, compare all v1 mask bits, VMX emulator semantics; no hardware exception-status claim','object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest()};(out/'aabb-mask-raw-comparison.json').write_text(json.dumps(r,indent=2)+'\n');print('cases',512,'mismatches',len(bad),bad[:1]);assert not bad
