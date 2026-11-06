from pathlib import Path
import json,random,sys,hashlib
P=Path(r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow');sys.path.insert(0,str(P/'scratch/x360_sdk'));import native_atomic_fixture as F
class W(F.AtomicCPU):
 def exec_vector(self,pc,w,m):
  if pc==self.stop:raise F.CPU.run.__globals__['Return']()
  return super().exec_vector(pc,w,m)
D=Path(__file__).parent;obj=Path(json.loads((D/'compile-results.json').read_text())['results'][next(i for i,x in enumerate(json.loads((D/'compile-results.json').read_text())['results']) if x['source'].endswith('BrnCrashNavMap.cpp'))]['object']);sym='?OnEnter@CrashNavMap@BrnGui@@';b=F.Bundle([F.Coff(obj,sym)]);b.select(sym);base=F.M.S.SELF;aux=F.M.S.AUX;rng=random.Random(808)
for i in range(64):
 raw=rng.randbytes(0x6200);out=[]
 for native in [False,True]:
  c=F.cpu(b);c.__class__=W;c.mem.wr(base,raw);F.put(c,aux,0);c.setg(31,base);c.setg(30,0);c.setg(27,aux if native else 12);c.setg(28,1);c.setg(25,1);c.setg(26,0);c.setg(11,0);c.setg(10,0);c.setf(31,0.);c.vr[0]=(0,)*4;c.stop=b.entry+0x244 if native else 0x824cb3f8;c.run(b.entry+0x1d8 if native else 0x824cb360);out.append(c.mem.rd(base+0x6100,16))
 assert out[0]==out[1]==bytes(16),(i,out)
report={'passed':True,'cases':64,'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'Selected actual OnEnter write window vs ARTIST824CB360..3F4: full locked-icon position16 bytes become zero from randomized original bytes. Register/window setup controlled; remaining OnEnter behavior excluded.'};(D/'onenter-zero-comparison.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
