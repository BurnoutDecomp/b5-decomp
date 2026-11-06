from pathlib import Path
import json,random,sys,hashlib
P=Path(r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow');sys.path.insert(0,str(P/'scratch/x360_sdk'));import native_atomic_fixture as F
class W(F.AtomicCPU):
 def exec_vector(self,pc,w,m):
  if pc==self.stop:raise F.CPU.run.__globals__['Return']()
  return super().exec_vector(pc,w,m)
D=Path(__file__).parent;rs=json.loads((D/'compile-results.json').read_text())['results'];paths={r['source']:Path(r['object']) for r in rs};base=F.M.S.SELF;aux=F.M.S.AUX;rng=random.Random(346);obj=next(v for k,v in paths.items() if k.endswith('BrnGuiEventTypeDefs.cpp'));b=F.Bundle([F.Coff(obj,'?Construct@GuiHudMessage@BrnGui@@QAAXPBD@Z')]);b.select('?Construct@GuiHudMessage@BrnGui@@QAAXPBD@Z');c=F.cpu(b);pc=0x824f7948;w=int.from_bytes(c.mem.rd(pc,4),'big');d=w&0x03fffffc;d=d-(1<<26) if d&(1<<25) else d;orig=(pc+d)&0xffffffff
for i in range(32):
 raw=rng.randbytes(840);hv=rng.getrandbits(64);outs=[]
 for native in [False,True]:
  c=F.cpu(b);c.mem.wr(base,raw);c.mem.wr(aux,b'HUD_test\0');c.hooks[orig]=lambda x:x.setg(3,hv)
  for a,n in b.unresolved.items():
   if 'CgsIDCompress' in n:c.hooks[a]=lambda x:x.setg(3,hv)
  c.setg(3,base);c.setg(4,aux);c.run(b.entry if native else 0x824f78b0);outs.append(c.mem.rd(base,840))
 assert outs[0]==outs[1],('ctor',i)
obj2=next(v for k,v in paths.items() if k.endswith('BrnGuiHudMessageDirector_gUI_00.cpp'));b=F.Bundle([F.Coff(obj2,'?FilterAndSendOffMessage@HudMessageDirector@BrnGui@@')]);b.select('?FilterAndSendOffMessage@HudMessageDirector@BrnGui@@')
for i in range(32):
 raw=rng.randbytes(0x5500);event=rng.randbytes(840);ticks=rng.getrandbits(64);outs=[]
 for native in [False,True]:
  c=F.cpu(b);c.__class__=W;c.mem.wr(base,raw);c.mem.wr(aux,event);c.setg(27,base);c.setg(31,base);c.setg(26,aux);c.setg(29,aux);c.setg(11,0x4b68+(i%300)*8);c.setg(10,ticks);c.stop=b.entry+0x1c0 if native else 0x82511834;c.run(b.entry+0x1ac if native else 0x82511820);outs.append((tuple(c.g(x)&0xffffffff for x in [3,4,5,6]),c.mem.rd(base,0x5500)))
 assert outs[0]==outs[1],('enqueue',i,outs[0][0],outs[1][0])
report={'passed':True,'construct_cases':32,'enqueue_cases':32,'constructor_object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'enqueue_object_sha256':hashlib.sha256(obj2.read_bytes()).hexdigest(),'scope':'Original and actual constructor 840 bytes (hash service controlled) and enqueue window (id154, size840, pointer, timestamp), queue body and full runtime excluded.'};(D/'hud-comparison.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
