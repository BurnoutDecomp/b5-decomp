import sys,json,random,hashlib
from pathlib import Path
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave7/dac-terra');sys.path.insert(0,str(out));from weak_bundle import F,WeakBundle
d=out/'profiler-lifecycle-native-build';ps=[d/'System.obj',d/'Profiler.obj'];b=WeakBundle([F.Coff(ps[0],'?GetProfiler@System@'),F.Coff(ps[1],'?Release@Profiler@')]);b.select('?GetProfiler@System@');release=F.address(b,'?Release@Profiler@');oi,v,sec=b.definitions['??_7Profiler@core@audio@rw@@6B@'];vt=b.bases[(oi,sec)]+v;errors=[];rng=random.Random(0xa9b0);addr=F.M.S.SELF
for i in range(128):
 initial=rng.randbytes(0x100);both=[]
 for compiled in (False,True):
  c=F.cpu(b);c.mem.wr(addr,initial);F.put(c,addr,vt if compiled else 0x8214b260);c.setg(3,addr);calls=[]
  for at,n in b.unresolved.items():
   if '??3@' in n:c.hooks[at]=lambda x:calls.append('delete')
  c.hooks[0x82815570]=lambda x:calls.append('delete');c.run(release if compiled else 0x82b6a9b0);result=bytearray(c.mem.rd(addr,0x100));result[:4]=bytes(4);both.append((result,calls))
 if both[0]!=both[1]:errors.append(i)
r={'cases':128,'mismatches':errors,'objects':{p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in ps},'limits':'Actual Release executes native virtual slot1 and deleting destructor with flags 0; object bytes normalized only for vtable address, delete hook detects forbidden free. No shutdown concurrency or hardware claim.'};(out/'profiler-lifecycle-release-comparison.json').write_text(json.dumps(r,indent=2)+'\n');print('cases 128 mismatches',errors);assert not errors


