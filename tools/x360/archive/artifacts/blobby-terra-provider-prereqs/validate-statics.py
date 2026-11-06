from pathlib import Path
import json,random,sys,hashlib
P=Path(r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow');sys.path.insert(0,str(P/'scratch/x360_sdk'));import native_atomic_fixture as F
D=Path(__file__).parent;obj=Path(json.loads((D/'compile-results.json').read_text())['results'][0]['object']);rng=random.Random(44);cases=0
for name,orig,globaladdr in [('kvShadowOffset',0x82c4ed48,0x82faefb0),('kfAmbientShadowFadeDistance',0x82c4ed88,0x82faf0e0),('kfAmbientShadowScaleFactor',0x82c4edb0,0x82fafbf0)]:
 sym='??__E'+name;o=F.Coff(obj,sym);b=F.Bundle([o]);b.select(sym);loc=[b.bases[(0,s)]+v for n,v,s in o.symbols.values() if n==name and (0,s) in b.bases];assert len(loc)==1,(name,loc)
 for i in range(32):
  initial=bytes(rng.getrandbits(8) for _ in range(16));outs=[]
  for native in (False,True):
   c=F.cpu(b);c.mem.add(0x82fae000,0x2000);c.mem.wr(globaladdr,initial);c.mem.wr(loc[0],initial);c.run(b.entry if native else orig);outs.append(c.mem.rd(loc[0] if native else globaladdr,16))
  assert outs[0]==outs[1],(name,i,[x.hex() for x in outs]);cases+=1
out={'passed':True,'cases':cases,'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'Three actual dynamic vector initializers vs ARTIST 82C4ED48/ED88/EDB0; randomized prior bytes, compare all 16 final bytes of each named static. Per-initializer only; CRT registration order and runtime integration excluded.'};(D/'static-initializers-comparison.json').write_text(json.dumps(out,indent=2)+'\n');print(out)
