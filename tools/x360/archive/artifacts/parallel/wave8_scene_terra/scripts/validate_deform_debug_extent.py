from pathlib import Path
import sys,json,random,struct,hashlib
sys.path.insert(0,r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\scratch\x360_sdk');import native_atomic_fixture as F
p=Path('D:/Reverse/BP-X360-Experimental-Artifacts/parallel/wave8_scene_terra/deformation-next');obj=p/'9.obj';n='?GetExtent@DeformationAxisAlignedBox@';o=F.Coff(obj,n);b=F.Bundle([o]);b.select(n);A=0x40000000;rng=random.Random(0x825dec50);bad=[]
for i in range(128):
 data=struct.pack('>8f',*[rng.uniform(-10,10) for _ in range(8)])
 for direction in range(8):
  results=[]
  for comp in [False,True]:
   c=F.cpu(b);c.mem.add(A,0x100);c.mem.wr(A,data);c.setg(3,A);c.setg(4,direction);c.run(b.entry if comp else 0x825dec50);results.append(struct.pack('>d',c.f(1)).hex())
  if results[0]!=results[1]:bad.append((i,direction,results));break
print('mismatches',len(bad));(p/'debug-extent-comparison.json').write_text(json.dumps({'cases':1024,'mismatches':bad,'scope':'whole GetExtent 825DEC50; finite arbitrary box lanes, six directions and two out-of-range values, compares f1; no nonfinite/hardware claim','object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest()},indent=2)+'\n');assert not bad
