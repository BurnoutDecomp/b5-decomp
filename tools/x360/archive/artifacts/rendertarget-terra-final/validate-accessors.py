from pathlib import Path
import sys,json,hashlib,random
P=Path(r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow');sys.path.insert(0,str(P/'scratch/x360_sdk'));import native_atomic_fixture as F
D=Path(__file__).parent;obj=Path(json.loads((D/'compile-results.json').read_text())['results'][0]['object']);rng=random.Random(42);cases=0
for sym,pc,src,dst,offset in [('?GetTexture@RenderTarget@postfx@graphics@rw@@',0x8240a548,29,23,0x28),('?GetDepthTextureState@RenderTarget@postfx@graphics@rw@@',0x8240a5e4,7,25,0x8c)]:
 b=F.Bundle([F.Coff(obj,sym)]);b.select(sym)
 for i in range(96):
  value=rng.getrandbits(32);outs=[]
  for native in (False,True):
   c=F.cpu(b);base=F.M.S.SELF;F.put(c,base+offset,value)
   if native:c.setg(3,base);c.setg(4,0);c.run(b.entry);outs.append(c.g(3))
   else:c.setg(src,base);w=c.fetch(pc);c.step(pc,w,pc+4,0);outs.append(c.g(dst))
  assert outs[0]==outs[1],(sym,i,outs);cases+=1
out={'passed':True,'cases':cases,'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'Actual RenderTarget colour index-zero and depth-state accessor outputs vs original inlined BrnPostFx loads 8240A548 and 8240A5E4, using arbitrary pointer bits. Other colour indices follow declared Target array stride but were not original-call-site tested; no runtime claim.'};(D/'postfx-accessor-comparison.json').write_text(json.dumps(out,indent=2)+'\n');print(out)
