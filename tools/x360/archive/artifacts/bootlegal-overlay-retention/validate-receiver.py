from pathlib import Path
import sys,json,hashlib,random
P=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(P/'scratch/x360_sdk'));import native_atomic_fixture as F
D=Path('D:/Reverse/BP-X360-Experimental-Artifacts/bootlegal-overlay-retention');j=json.loads((D/'compile-results.json').read_text());obj=Path(next(x for x in j['results'] if x['source'].endswith('BrnGuiMovieManager.cpp'))['object']);sym='?RecvEvent@MovieManager@BrnGui@@';o=F.Coff(obj,sym);b=F.Bundle([o]);b.select(sym);rng=random.Random(82496)
for i in range(32):
 payload=bytes(rng.getrandbits(8) for _ in range(48));outs=[]
 for native in (False,True):
  c=F.cpu(b);base=0x51000000;event=0x52000000;c.mem.add(base,0x4000);c.mem.add(event,0x1000);c.mem.wr(event,payload);c.mem.wr(base+0x141c,b'\x01');trace=[]
  def copy(x):trace.append((x.g(3)&0xffffffff,x.g(4)&0xffffffff,x.mem.rd(x.g(4)&0xffffffff,48)))
  c.hooks[0x824eafd8]=copy
  for n,v,sec in o.symbols.values():
   if '?Copy@VideoDefinition@MovieManager@BrnGui@@' in n and (0,sec) in b.bases:c.hooks[b.bases[(0,sec)]+v]=copy
  for a,n in b.unresolved.items():
   if '?Copy@VideoDefinition@MovieManager@BrnGui@@' in n:c.hooks[a]=copy
  c.setg(3,base);c.setg(4,event);c.setg(5,508);c.run(b.entry if native else 0x824f9688);outs.append((trace,c.mem.rd(base+0x141c,1)))
 assert outs[0]==outs[1],(i,outs)
out={'passed':True,'cases':32,'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'Actual MovieManager::RecvEvent 508 vs ARTIST 824F9688, same receiver/payload addresses, compares raw Copy receiver and source pointer/48-byte payload plus stop-flag store. Copy controlled; non-null event only, no other state/runtime claim.'};(D/'video-receiver-comparison.json').write_text(json.dumps(out,indent=2)+'\n');print(out)
