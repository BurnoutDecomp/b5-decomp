from pathlib import Path
import json,random,sys,hashlib,struct
P=Path(r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow');sys.path.insert(0,str(P/'scratch/x360_sdk'));import native_atomic_fixture as F
class W(F.AtomicCPU):
 def exec_vector(self,pc,w,m):
  if pc==self.stop:raise F.CPU.run.__globals__['Return']()
  return super().exec_vector(pc,w,m)
D=Path(__file__).parent;r=json.loads((D/'compile-results.json').read_text())['results'];obj=Path(next(x['object'] for x in r if x['source'].endswith('BrnCrashNavMap.cpp')));provider=Path(next(x['object'] for x in r if x['source'].endswith('BrnMapUtils.cpp')));sym='?Transform@MapTransform@BrnGui@@SA?AUVector2@vpu@math@rw@@U3456@VMatrix33@456@1@Z';b=F.Bundle([F.Coff(obj,'?PlaceCursorOnPlayer@CrashNavMap@BrnGui@@'),F.Coff(provider,sym)]);b.select('?PlaceCursorOnPlayer@CrashNavMap@BrnGui@@');oi,v,s=b.definitions[sym];target=b.bases[(oi,s)]+v;devname=next(n for n in b.definitions if n.startswith('?smm33DeviceSpace@MapTransform'));oi,v,s=b.definitions[devname];devloc=b.bases[(oi,s)]+v;base=F.M.S.SELF;cache=0x86000000;rng=random.Random(227)
for i in range(32):
 pts=[rng.uniform(-100,100) for _ in range(4)];world=[rng.uniform(-100,100) for _ in range(4)];view=[rng.uniform(-100,100) for _ in range(4)];device=[rng.uniform(-5,5) for _ in range(12)];outs=[]
 for native in (False,True):
  c=F.cpu(b);c.__class__=W;c.mem.add(cache,0x10000);c.mem.add(0x82fb3000,0x1000);F.put(c,base+0x48,cache);c.mem.wr(cache+0x4ae0,struct.pack('>4f',*pts));c.mem.wr(base+0x670,struct.pack('>4f',*world));c.mem.wr(base+0x680,struct.pack('>4f',*view));dev=struct.pack('>12f',*device);c.mem.wr(0x82fb3050,dev);c.mem.wr(devloc,dev)
  def player(x):x.setg(3,cache+0x4ae0)
  def cp(x):
   dest=x.g(3)&0xffffffff;x.mem.wr(dest,x.mem.rd(x.g(4)&0xffffffff,x.g(5)));x.setg(3,dest)
  for a,n in b.unresolved.items():
   if 'GetWorldCameraPosition@GuiCache' in n:c.hooks[a]=player
   elif n=='memcpy':c.hooks[a]=cp
  c.setg(3,base);c.stop=b.entry+0x188 if native else 0x824bf908;c.run(b.entry if native else 0x824bf6f0)
  fr=c.mem.rd(c.g(3)&0xffffffff,48);to=c.mem.rd(c.g(4)&0xffffffff,48);point=tuple(c.vr[1]);c.stop=0;c.run(target if native else 0x824503c0);outs.append((point,fr,to,tuple(c.vr[1])))
 assert outs[0]==outs[1],(i,outs)
r={'passed':True,'cases':32,'caller_object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'provider_object_sha256':hashlib.sha256(provider.read_bytes()).hexdigest(),'scope':'Actual PlaceCursorOnPlayer and real native MapTransform provider vs ARTIST 824BF6F0->824503C0; capture live v1, read original/native matrix arguments through live r3/r4, then execute real corresponding provider with those same registers unchanged and compare full v1 output. Finite random input; GuiCache accessor controlled, no full game/frame/hardware-estimate claim.'};(D/'place-caller-provider-comparison.json').write_text(json.dumps(r,indent=2)+'\n');print(r)
