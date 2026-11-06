from pathlib import Path
import json,random,sys,hashlib,struct
P=Path(r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow');sys.path.insert(0,str(P/'scratch/x360_sdk'));import native_atomic_fixture as F
class W(F.AtomicCPU):
 def exec_vector(self,pc,w,m):
  if pc==self.stop:raise F.CPU.run.__globals__['Return']()
  return super().exec_vector(pc,w,m)
D=Path(__file__).parent;obj=Path(json.loads((D/'compile-results.json').read_text())['results'][0]['object']);sym='?PlaceCursorOnPlayer@CrashNavMap@BrnGui@@';b=F.Bundle([F.Coff(obj,sym)]);b.select(sym);base=F.M.S.SELF;cache=0x86000000;rng=random.Random(227)
for i in range(32):
 pts=[rng.uniform(-100,100) for _ in range(4)];world=[rng.uniform(-100,100) for _ in range(4)];view=[rng.uniform(-100,100) for _ in range(4)];device=[rng.uniform(-5,5) for _ in range(12)];outs=[]
 for native in [False,True]:
  c=F.cpu(b);c.__class__=W;c.mem.add(cache,0x10000);c.mem.add(0x82fb3000,0x1000);F.put(c,base+0x48,cache);c.mem.wr(cache+0x4ae0,struct.pack('>4f',*pts));c.mem.wr(base+0x670,struct.pack('>4f',*world));c.mem.wr(base+0x680,struct.pack('>4f',*view));dev=struct.pack('>12f',*device);c.mem.wr(0x82fb3050,dev)
  def player(x):x.setg(3,cache+0x4ae0)
  def cp(x):
   dest=x.g(3)&0xffffffff;x.mem.wr(dest,x.mem.rd(x.g(4)&0xffffffff,x.g(5)));x.setg(3,dest)
  for a,n in b.unresolved.items():
   if 'GetWorldCameraPosition@GuiCache' in n:c.hooks[a]=player
   elif 'smm33DeviceSpace@MapTransform' in n:c.mem.wr(a,dev)
   elif n=='memcpy':c.hooks[a]=cp
  c.setg(3,base);c.stop=b.entry+0x1b4 if native else 0x824bf908;c.run(b.entry if native else 0x824bf6f0)
  if native:
   sp=c.g(1)&0xffffffff;fr=c.mem.rd(sp+0x80,48);to=c.mem.rd(sp+0xb0,48)
  else:fr=c.mem.rd(c.g(3)&0xffffffff,48);to=c.mem.rd(c.g(4)&0xffffffff,48)
  outs.append((tuple(c.vr[1]),fr,to))
 assert outs[0]==outs[1],(i,outs[0][0],outs[1][0],[(hex(k),a,z) for k,(a,z) in enumerate(zip(outs[0][1],outs[1][1])) if a!=z][:20],[(hex(k),a,z) for k,(a,z) in enumerate(zip(outs[0][2],outs[1][2])) if a!=z][:20])
report={'passed':True,'cases':32,'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'Original824BF744..908 vs actual native PlaceCursorOnPlayer up to Transform call; finite randomized player/world/view/device lanes, compares v1 and two actual 48-byte matrices (native locals, original argument pointers). This explicitly does NOT validate the currently wrong native Matrix33 by-value call ABI; memcpy/cache accessor controlled, no full function/runtime claim.'};(D/'place-arguments-comparison.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
