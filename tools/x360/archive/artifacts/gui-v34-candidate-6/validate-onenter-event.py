from pathlib import Path
import json,random,sys,hashlib
P=Path(r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow');sys.path.insert(0,str(P/'scratch/x360_sdk'));import native_atomic_fixture as F
class W(F.AtomicCPU):
 def exec_vector(self,pc,w,m):
  if pc==self.stop:raise F.CPU.run.__globals__['Return']()
  return super().exec_vector(pc,w,m)
D=Path(__file__).parent;obj=Path(json.loads((D/'compile-results.json').read_text())['results'][0]['object']);sym='?OnEnter@CrashNavMap@BrnGui@@';b=F.Bundle([F.Coff(obj,sym)]);b.select(sym);base=F.M.S.SELF;state=0x86100000;rng=random.Random(591)
for i in range(32):
 initial=rng.randbytes(16);outs=[]
 for native in [False,True]:
  c=F.cpu(b);c.__class__=W;c.mem.add(state,0x1000);F.put(c,base+0x1c,state);c.setg(31,base);c.setg(30,0);c.setg(29,base+0x60);c.setg(28,base+0x60 if native else 1);sp=c.g(1)&0xffffffff;offset=0x50 if native else 0x60;c.mem.wr(sp+offset,initial)
  def get(x):x.setg(3,(x.g(3)+12)&0xffffffff)
  for a,n in b.unresolved.items():
   if 'GetOutputEventQueue@StateInterface' in n:c.hooks[a]=get
  c.stop=b.entry+0xe8 if native else 0x824cb284;c.run(b.entry+0xa8 if native else 0x824cb248);outs.append(((c.g(3)&0xffffffff,c.g(5)&0xffffffff,c.g(6)&0xffffffff),c.mem.rd(sp+offset,16)))
 assert outs[0]==outs[1] and outs[0][1][12:]==initial[12:],(i,outs,initial.hex())
report={'passed':True,'cases':32,'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'Actual OnEnter event555 construction window vs ARTIST824CB248..284, randomized initial stack record; channel40/size16/three header words and untouched final4 bytes compared; GetOutputEventQueue controlled. Rest of function/queue excluded.'};(D/'onenter-event-comparison.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
