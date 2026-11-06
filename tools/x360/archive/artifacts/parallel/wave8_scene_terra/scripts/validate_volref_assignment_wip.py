import sys,random,json,hashlib
from pathlib import Path
sys.path.insert(0,r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\scratch\x360_sdk');import native_atomic_fixture as F
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/parallel/wave8_scene_terra/volume-baseline');obj=out/'VolRef-typed-wip.obj';b=F.Bundle([F.Coff(obj,'??4VolRef@collision@rw@@')]);b.select('??4VolRef@collision@rw@@');rng=random.Random(0x82bb33e8);A=0x50000000;B=0x50001000;bad=[]
for i in range(128):
 dst=rng.randbytes(0x100);src=rng.randbytes(0x100);r=[]
 for compiled in (False,True):
  c=F.cpu(b);c.mem.add(A,0x100,dst);c.mem.add(B,0x100,src);c.setg(3,A);c.setg(4,B);c.run(b.entry if compiled else 0x82bb33e8);r.append((c.mem.rd(A,0x100),c.mem.rd(B,0x100),c.g(3)&0xffffffff))
 if r[0]!=r[1]:bad.append([i,r[0][0].hex(),r[1][0].hex()])
print('cases 128 mismatches',len(bad),bad[:1]);assert not bad
(out/'volref-assignment-wip-comparison.json').write_text(json.dumps({'cases':128,'mismatches':bad,'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'actual whole VolRef assignment vs 82BB33E8, arbitrary raw source/destination/guards and untouched alignment gaps, nonoverlapping records'},indent=2))
