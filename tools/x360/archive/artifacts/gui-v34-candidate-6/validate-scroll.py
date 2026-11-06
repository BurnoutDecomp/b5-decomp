from pathlib import Path
import json,random,sys,hashlib,struct
P=Path(r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow');sys.path.insert(0,str(P/'scratch/x360_sdk'));import native_atomic_fixture as F
class W(F.AtomicCPU):
 def exec_vector(self,pc,w,m):
  if pc==self.stop:raise F.CPU.run.__globals__['Return']()
  return super().exec_vector(pc,w,m)
D=Path(__file__).parent;obj=Path(json.loads((D/'compile-results.json').read_text())['results'][0]['object']);sym='?UpdateMainMap@CrashNavMap@BrnGui@@';b=F.Bundle([F.Coff(obj,sym)]);b.select(sym);base=F.M.S.SELF;rng=random.Random(701)
for i in range(48):
 mode=[0,1,4][i%3];world=[rng.uniform(-500,500) for _ in range(4)];centre=[rng.uniform(-500,500) for _ in range(4)];velocity=[rng.uniform(-2,2) for _ in range(4)];raw=rng.randbytes(0x6200);worldbits=struct.unpack('>4I',struct.pack('>4f',*world));outs=[]
 for native in [False,True]:
  c=F.cpu(b);c.__class__=W;c.mem.wr(base,raw);F.put(c,base+0x5f50,mode);c.mem.wr(base+0x60f0,struct.pack('>4f',*centre));c.mem.wr(base+0x6070,struct.pack('>4f',*velocity))
  def dtw(x):x.vr[1]=worldbits
  def flatten(x):v=x.vr[1];x.vr[1]=(v[0],v[2],v[0],v[0])
  c.hooks[0x824bab78]=dtw
  for a,n in b.unresolved.items():
   if 'DeviceToWorld@MapTransform' in n:c.hooks[a]=dtw
   elif 'Flatten@MapTransform' in n:c.hooks[a]=flatten
  c.setg(3,base);c.stop=b.entry+0x1cc if native else 0x824d8e78;c.run(b.entry if native else 0x824d8cb0);outs.append((c.mem.rd(base,0x6200),c.g(4)&0xffffffff))
 assert outs[0]==outs[1],(i,mode,outs[0][1],outs[1][1],[(hex(k),a,z) for k,(a,z) in enumerate(zip(outs[0][0],outs[1][0])) if a!=z][:20])
report={'passed':True,'cases':48,'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'Actual UpdateMainMap through UpdateSoundEvents call vs ARTIST824D8CB0, finite inputs, three cursor modes and all four lanes varied; DeviceToWorld/Flatten controlled. Reciprocal-square-root estimate model, not physical Xenon; other rendering/runtime excluded.'};(D/'scroll-comparison.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
