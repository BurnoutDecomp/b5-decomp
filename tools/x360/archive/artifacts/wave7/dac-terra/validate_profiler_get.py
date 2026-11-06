import sys,json,random,struct,hashlib
from pathlib import Path
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave7/dac-terra');sys.path.insert(0,str(out));from weak_bundle import F,WeakBundle
d=out/'profiler-native-build';objs=[d/'System.obj',d/'Profiler.obj'];b=WeakBundle([F.Coff(objs[0],'?GetProfiler@System@'),F.Coff(objs[1],'?Start@Profiler@')]);b.select('?GetProfiler@System@');matches=[b.bases[(oi,sec)]+v for oi,o in enumerate(b.objects) for n,v,sec in o.symbols.values() if n=='skProfiler' and (oi,sec) in b.bases];assert len(matches)==1,matches;compiledprof=matches[0];origprof=0x832718f0;errors=[];rng=random.Random(0xde08);selfaddr=F.M.S.SELF;aux=F.M.S.AUX
for i in range(128):
 system=bytearray(rng.randbytes(0x1200));prof=bytearray(rng.randbytes(0x38));existing=aux+0x7000 if i&1 else 0;struct.pack_into('>I',system,0x34,existing);both=[]
 for compiled in (False,True):
  c=F.cpu(b);c.mem.add(0x83271800,0x200);c.mem.wr(selfaddr,system);dest=compiledprof if compiled else origprof;c.mem.wr(dest,prof);c.setg(3,selfaddr);c.run(b.entry if compiled else 0x82b6de08);obj=bytearray(c.mem.rd(selfaddr,0x1200));state=bytearray(c.mem.rd(dest,0x38));got=c.g(3)&0xffffffff
  if not existing:
   assert got==dest;assert int.from_bytes(obj[0x34:0x38],'big')==dest;obj[0x34:0x38]=bytes(4);state[:4]=bytes(4)
  both.append((obj,state,existing if existing else 0))
 if both[0]!=both[1]:errors.append({'case':i,'sysdiff':[hex(j) for j,(a,z) in enumerate(zip(both[0][0],both[1][0])) if a!=z][:20],'profdiff':[hex(j) for j,(a,z) in enumerate(zip(both[0][1],both[1][1])) if a!=z][:20]})
r={'cases':128,'errors':errors,'objects':{p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in objs},'limits':'Actual System::GetProfiler, cold/cache paths and randomized existing singleton bytes, original/compiled final state and return; vtable and singleton addresses normalized, no concurrency claim. C++ placement construction installs vtable before field initialization, whereas original stores two fields first.'};(out/'profiler-get-comparison.json').write_text(json.dumps(r,indent=2)+'\n');print(len(errors),errors[:1]);assert not errors
