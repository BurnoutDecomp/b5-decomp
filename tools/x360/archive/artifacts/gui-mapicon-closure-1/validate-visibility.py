from pathlib import Path
import json,random,sys,hashlib
P=Path(r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow');sys.path.insert(0,str(P/'scratch/x360_sdk'));import native_atomic_fixture as F
D=Path(r'D:\Reverse\BP-X360-Experimental-Artifacts\gui-mapicon-closure-1');r=json.loads((D/'compile-results.json').read_text())['results'][0];obj=Path(r['object']);sym='?SetIconsVisible@MapIconManager@BrnGui@@';b=F.Bundle([F.Coff(obj,sym)]);b.select(sym);base=F.M.S.SELF;rng=random.Random(847)
for i in range(32):
 raw=rng.randbytes(0xab00);visible=i%2;owner=(i//2)%3;outs=[]
 for native in [False,True]:
  c=F.cpu(b);c.mem.wr(base,raw);F.put(c,base+0xaa00,owner);trace=[]
  def sat(x):trace.append(('sat',x.g(3)&0xffffffff))
  def crash(x):trace.append(('crash',x.g(3)&0xffffffff))
  c.hooks[0x82522588]=sat;c.hooks[0x825212c0]=crash
  c.hooks[F.address(b,"?UpdateSatNavIcons@MapIconManager@BrnGui@@")]=sat;c.hooks[F.address(b,"?UpdateCrashNavIcons@MapIconManager@BrnGui@@")]=crash
  for at,n in b.unresolved.items():
   if 'UpdateSatNavIcons' in n:c.hooks[at]=sat
   if 'UpdateCrashNavIcons' in n:c.hooks[at]=crash
  c.setg(3,base);c.setg(4,visible);c.run(b.entry if native else 0x82525f18);outs.append((c.mem.rd(base,0xab00),trace))
 assert outs[0]==outs[1],(i,outs[0][1],outs[1][1],[(hex(k),a,z) for k,(a,z) in enumerate(zip(outs[0][0],outs[1][0])) if a!=z][:8])
report={'passed':True,'cases':32,'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'Whole actual SetIconsVisible vs ARTIST82525F18, both visibility values and three owner values, randomized manager; owner-specific Update calls controlled. Full icon rendering excluded.'};(D/'visibility-comparison.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
