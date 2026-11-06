from pathlib import Path
import json,random,sys,hashlib
P=Path(r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow');sys.path.insert(0,str(P/'scratch/x360_sdk'));import native_atomic_fixture as F
D=Path(__file__).parent;r=next(x for x in json.loads((D/'compile-results.json').read_text())['results'] if x['source'].endswith('BrnGuiCache.cpp'));obj=Path(r['object']);base=F.M.S.SELF;aux=F.M.S.AUX;rng=random.Random(744)
b=F.Bundle([F.Coff(obj,'??0GuiCache@BrnGui@@')]);b.select('??0GuiCache@BrnGui@@')
for i in range(16):
 raw=rng.randbytes(0x143f0);outs=[]
 for native in [False,True]:
  c=F.cpu(b);c.mem.wr(base,raw);c.setg(3,base);c.run(b.entry if native else 0x827e05b8);outs.append(c.mem.rd(base,len(raw)))
 assert outs[0]==outs[1],('ctor',i)
b=F.Bundle([F.Coff(obj,'?RecEvent@GuiCache@BrnGui@@')]);b.select('?RecEvent@GuiCache@BrnGui@@')
for case in [396,442,553]:
 for i in range(32):
  raw=rng.randbytes(0x143f0);ev=bytearray(rng.randbytes(0x300));outs=[]
  if case==396:ev[4:8]=(i%8).to_bytes(4,'big')
  if case==442:ev[0x220:0x224]=(i%6).to_bytes(4,'big');ev[:8]=(0x123456789abcdef0).to_bytes(8,'big')
  if case==553:ev[0]=[0,1,255][i%3]
  for native in [False,True]:
   c=F.cpu(b);c.mem.wr(base,raw);c.mem.wr(aux,ev);F.put(c,base+0x4b00,i%8);c.mem.wr(base+0xa0a0+(i%8)*8,(0x123456789abcdef0).to_bytes(8,'big'))
   def cp(x):
    dst=x.g(3);x.mem.wr(dst,x.mem.rd(x.g(4),x.g(5)));x.setg(3,dst)
   c.hooks[0x82c0a630]=cp
   for addr,name in b.unresolved.items():
    if name=='memcpy':c.hooks[addr]=cp
   c.setg(3,base);c.setg(4,aux);c.setg(5,case);c.run(b.entry if native else 0x8250ddf0);outs.append(c.mem.rd(base,0x143f0))
  assert outs[0]==outs[1],(case,i,[(hex(k),a,z) for k,(a,z) in enumerate(zip(*outs)) if a!=z][:15])
report={'passed':True,'constructor_cases':16,'event_cases':96,'events':[396,442,553],'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'Actual whole-source constructor and RecEvent vs ARTIST 827E05B8 and 8250DDF0; valid bounded indices/counts, randomized cache bytes, raw bool bytes; memcpy controlled. Other events, failure assertions and full runtime excluded.'};(D/'comparison.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
