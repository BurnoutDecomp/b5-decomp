from pathlib import Path
import json,random,sys,hashlib
P=Path(r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow');sys.path.insert(0,str(P/'scratch/x360_sdk'));import native_atomic_fixture as F
D=Path(__file__).parent;obj=Path(json.loads((D/'compile-results.json').read_text())['results'][0]['object']);sym='?Construct@SatNavMapIcon@BrnGui@@';b=F.Bundle([F.Coff(obj,sym)]);b.select(sym);base=F.M.S.SELF+0x100;aux=F.M.S.AUX;rng=random.Random(177)
for i in range(32):
 raw=rng.randbytes(0x200);outs=[]
 for native in [False,True]:
  c=F.cpu(b);c.mem.wr(base-32,raw);trace=[]
  def ct(x):trace.append(tuple(x.g(n)&0xffffffff for n in [3,4,5,6]))
  c.hooks[0x8241c1f0]=ct
  for a,n in b.unresolved.items():
   if 'Construct@FlaptIconComponent' in n:c.hooks[a]=ct
  c.setg(3,base);c.setg(4,aux);c.setg(5,aux+0x100);c.setg(6,aux+0x200);c.run(b.entry if native else 0x82448340);outs.append((c.mem.rd(base-32,len(raw)),trace))
 assert outs[0]==outs[1],(i,outs[0][1],outs[1][1],[(hex(k),a,z) for k,(a,z) in enumerate(zip(outs[0][0],outs[1][0])) if a!=z][:10])
report={'passed':True,'cases':32,'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'Actual SatNavMapIcon::Construct vs ARTIST82448340, randomized component bytes; FlaptIconComponent Construct controlled, valid pointers. FLT_MAX original8204F664=7F7FFFFF; assert paths and full frame excluded.'};(D/'satnav-construct-comparison.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
