from pathlib import Path
import json,random,struct,sys,hashlib
P=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(P/'scratch/x360_sdk'));import native_atomic_fixture as F
D=Path('D:/Reverse/BP-X360-Experimental-Artifacts/gamestate-gui-c0d-terra');rs=json.loads((D/'wip2/compile-results.json').read_text())['results'];p=Path(next(x['object'] for x in rs if x['source'].endswith('CgsAptRenderHandler.cpp')));q=Path(next(x['object'] for x in json.loads((D/'wip2/compile-results.json').read_text())['results'] if x['source'].endswith('CgsLinkedList.cpp')));t=Path(next(x['object'] for x in json.loads((D/'wip2/compile-results.json').read_text())['results'] if x['source'].endswith('CgsAptString.cpp')));sym='?Construct@AptRenderHandler@CgsGui@@';b=F.Bundle([F.Coff(p,sym),F.Coff(q,'?InternalInit@BaseLinkedList@'),F.Coff(t,'?Construct@CgsAptString@')]);b.select(sym);base=F.M.S.SELF;aux=F.M.S.AUX;rng=random.Random(2360);results=[]
for i in range(24):
 raw=rng.randbytes(0x1a840);aspect=0.5+i*0.13;out=[]
 for native in (False,True):
  c=F.cpu(b);c.mem.add(0x83010000,0x2000);c.mem.wr(base,raw);F.put(c,0x83010f58,aux+0x1000);trace=[]
  def text(x):trace.append(('text',x.g(3)&0xffffffff,x.g(4)&0xffffffff,x.g(5)&0xffffffff))
  def strctor(x):
   at=x.g(3)&0xffffffff;F.put(x,at+0x7c,x.g(4));F.put(x,at+0x80,x.g(5));text(x)
  def white(x):F.put(x,x.g(3)+0x80,x.g(4))
  c.hooks[0x827eee70]=text;c.hooks[0x828466e0]=white
  for at,n in b.unresolved.items():
   if 'Construct@TextObject@' in n:c.hooks[at]=text
   if 'mgStateLibrary' in n:F.put(c,at+0x38,aux+0x1000)
  for r,v in [(3,base),(4,aux+0x100),(5,aux+0x200),(6,aux+0x300),(7,aux+0x400),(9,aux+0x500),(10,3)]:c.setg(r,v)
  c.setf(1,aspect);c.run(b.entry if native else 0x82854430);out.append((c.mem.rd(base,len(raw)),trace))
 assert out[0]==out[1],(i,[(hex(k),x,y) for k,(x,y) in enumerate(zip(out[0][0],out[1][0])) if x!=y][:25],out[0][1][:2],out[1][1][:2]);results.append(i)
report={'passed':True,'cases':len(results),'object':str(p),'sha256':hashlib.sha256(p.read_bytes()).hexdigest(),'scope':'Actual production Construct versus ARTIST across24 finite aspect ratios and randomized full0x1A840 owner bytes. TextObject and white texture controlled, native string and list initialization real; reciprocal estimate is emulator model, assertions/physical hardware excluded.'};(D/'construct-comparison-wip2.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))

