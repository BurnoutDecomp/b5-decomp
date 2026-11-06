from pathlib import Path
import sys,json,hashlib,random
P=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(P/'scratch/x360_sdk'));import native_atomic_fixture as F
D=Path('D:/Reverse/BP-X360-Experimental-Artifacts/bootlegal-video-step3');j=json.loads((D/'compile-results.json').read_text());obj=Path(next(x for x in j['results'] if 'OutputGuiEvent_Inst.cpp' in x['source'])['object']);rng=random.Random(82476);reports={}
for name,address in [('Play',0x82476c68),('Stop',0x82476cf8)]:
 sym='??$OutputGuiEvent@VGuiEvent'+name+'Video@BrnGui@@@StateInterface@CgsGui@@';o=F.Coff(obj,sym);b=F.Bundle([o]);b.select(sym)
 for i in range(128):
  payload=bytes(rng.getrandbits(8) for _ in range(48));initial=bytes(rng.getrandbits(8) for _ in range(64));outs=[]
  for native in (False,True):
   c=F.cpu(b);iface=F.M.S.SELF;event=F.M.S.AUX;sp=c.g(1);c.mem.wr(event,payload);c.mem.wr(sp-0xa0+0x50,initial);trace=[]
   def hook(x):trace.append((x.g(3)&0xffffffff,x.g(5),x.g(6),x.mem.rd(x.g(4)&0xffffffff,x.g(6))))
   c.hooks[0x82435188]=hook
   for n,v,sec in o.symbols.values():
    if '?AddEvent@?$VariableEventQueue@$0BAAAA@$0BA@@' in n and (0,sec) in b.bases:c.hooks[b.bases[(0,sec)]+v]=hook
   for a,n in b.unresolved.items():
    if '?AddEvent@' in n:c.hooks[a]=hook
   c.setg(3,iface);c.setg(4,event);c.run(b.entry if native else address);outs.append(trace)
  assert outs[0]==outs[1],(name,i,outs)
 reports[name]=128
out={'passed':True,'cases':reports,'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'Actual whole play and stop OutputGuiEvent native methods vs ARTIST 82476C68/82476CF8 with identical randomized 48-byte source and preseeded 64-byte stack record. Compare queue receiver, channel, total size, all record bytes including untouched padding; AddEvent service controlled, no queue/runtime behavior claim.'};(D/'video-wrapper-comparison.json').write_text(json.dumps(out,indent=2)+'\n');print(out)
