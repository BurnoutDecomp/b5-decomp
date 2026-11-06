from pathlib import Path
import sys,json,hashlib
P=Path(r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow');sys.path.insert(0,str(P/'scratch/x360_sdk'));import native_atomic_fixture as F
D=Path(__file__).parent;obj=Path(json.loads((D/'compile-results.json').read_text())['results'][1]['object']);cases=0
for sym,orig in [('?GetTexture@CgsRenderTarget@@',0x827e74a8),('?GetDepthTexture@CgsRenderTarget@@',0x827e7528)]:
 b=F.Bundle([F.Coff(obj,sym)]);b.select(sym)
 for i in range(64):
  outs=[]
  for native in (False,True):
   c=F.cpu(b);base=F.M.S.SELF;target=F.M.S.AUX;state=target+0x1000;F.put(c,base+0x108,target);F.put(c,target+0x2c+24*(i%4),state);F.put(c,state+0x20,0x12000000+i);F.put(c,target+0x88,0x34000000+i);c.setg(3,base);c.setg(4,i%4);c.run(b.entry if native else orig);outs.append(c.g(3))
  assert outs[0]==outs[1],(sym,i,outs);cases+=1
out={'passed':True,'cases':cases,'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'Actual CgsRenderTarget colour/depth texture getters vs ARTIST 827E74A8/7528; all four colour indices and direct texture-state raster/depth pointers. Non-null valid state inputs; assertions, link, GPU and runtime excluded.'};(D/'cgs-getters-comparison.json').write_text(json.dumps(out,indent=2)+'\n');print(out)
