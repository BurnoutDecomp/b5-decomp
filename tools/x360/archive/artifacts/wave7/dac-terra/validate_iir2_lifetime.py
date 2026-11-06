import sys,json,random,hashlib
from pathlib import Path
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave7/dac-terra');sys.path.insert(0,str(out));from weak_bundle import F,WeakBundle
d=out/'iir2-build';errors=[];rows=[];rng=random.Random(0x1998);selfaddr=F.M.S.SELF
for klass,orig,vt in [('LowPassIir2',0x82ba1998,0x8217f464),('HighPassIir2',0x82ba1830,0x8217f414),('BandPassIir2',0x82ba1638,0x8217f3a4),('LowShelfIir2',0x82ba19e0,0x8217f474),('HighShelfIir2',0x82ba1878,0x8217f424),('PeakingIir2',0x82ba1b90,0x8217f4d4)]:
 probe='?Native'+klass+'Lifetime';p=d/(klass+'.obj');b=WeakBundle([F.Coff(d/'iir2_probe.obj',probe),F.Coff(p,'?CreateInstance@'+klass+'@')]);b.select(probe);weak=next(x for x in b.weak_resolved if x[0].startswith('??_E'+klass+'@'));basevts={b.bases[(oi,sec)]+v for oi,o in enumerate(b.objects) for n,v,sec in o.symbols.values() if n.startswith('??_7PlugIn@') and (oi,sec) in b.bases}
 for i in range(64):
  initial=bytearray(rng.randbytes(0x200));both=[]
  for compiled in (False,True):
   c=F.cpu(b);c.mem.wr(selfaddr,initial);c.setg(3,selfaddr);c.setg(4,i&1);trace=[]
   def free(x):trace.append(x.g(3)&0xffffffff)
   c.hooks[0x82c08fb0]=free
   for at,n in b.unresolved.items():
    if n.startswith('??3@YAXPAX'):c.hooks[at]=free
   if not compiled:F.put(c,selfaddr,vt)
   c.run(b.entry if compiled else orig);got=int.from_bytes(c.mem.rd(selfaddr,4),'big');assert got in (basevts if compiled else {0x820aa810});memory=bytearray(c.mem.rd(selfaddr,0x200));memory[:4]=bytes(4);both.append((memory,trace))
  if both[0]!=both[1]:errors.append({'class':klass,'case':i})
 rows.append({'class':klass,'original':hex(orig),'weak_fallback':weak,'sha256':hashlib.sha256(p.read_bytes()).hexdigest()})
r={'cases':384,'errors':errors,'rows':rows,'limits':'Actual vtable slot3 scalar destructor flags 0/1 against original; operator delete controlled, weak SEARCH_LIBRARY fallback, base-vtable identities verified then normalized; no array delete/hardware/full Voice claim.'};(out/'iir2-lifetime-comparison.json').write_text(json.dumps(r,indent=2)+'\n');print('384 cases',len(errors),errors[:2]);assert not errors
