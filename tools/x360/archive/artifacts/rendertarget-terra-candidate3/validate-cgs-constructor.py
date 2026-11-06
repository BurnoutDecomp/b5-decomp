from pathlib import Path
import sys,json,hashlib,random
P=Path(r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow');sys.path.insert(0,str(P/'scratch/x360_sdk'));import native_atomic_fixture as F
D=Path(__file__).parent;obj=Path(json.loads((D/'compile-results.json').read_text())['results'][1]['object']);sym='??0CgsRenderTarget@@QAA@XZ';b=F.Bundle([F.Coff(obj,sym)]);b.select(sym);rng=random.Random(33)
for i in range(96):
 initial=bytes(rng.getrandbits(8) for _ in range(0x110));outs=[]
 for native in (False,True):
  c=F.cpu(b);base=F.M.S.SELF;c.mem.wr(base,initial);c.setg(3,base);c.run(b.entry if native else 0x823f47a0);outs.append((c.g(3),c.mem.rd(base+4,0x10c)))
 assert outs[0]==outs[1],(i,[(x[0],x[1].hex()) for x in outs])
out={'passed':True,'cases':96,'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'Actual CgsRenderTarget constructor vs ARTIST 823F47A0 with randomized initial 0x110 bytes; compares return and all post-vptr bytes including untouched fields. Vtable identity/layout separately known; vptr address excluded because relocation differs. No runtime claim.'};(D/'cgs-constructor-comparison.json').write_text(json.dumps(out,indent=2)+'\n');print(out)
