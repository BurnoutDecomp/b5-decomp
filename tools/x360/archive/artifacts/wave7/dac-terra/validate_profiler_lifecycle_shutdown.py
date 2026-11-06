import sys,json,random,hashlib
from pathlib import Path
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave7/dac-terra');sys.path.insert(0,str(out));from weak_bundle import F,WeakBundle
d=out/'profiler-lifecycle-native-build';ps=[d/'profiler_shutdown_probe.obj',d/'System.obj',d/'Profiler.obj'];b=WeakBundle([F.Coff(ps[0],'?ProfilerShutdownProbe'),F.Coff(ps[1],'?GetProfiler@System@'),F.Coff(ps[2],'?Release@Profiler@')]);b.select('?ProfilerShutdownProbe');sr=F.address(b,'?Release@System@');oi,v,sec=b.definitions['??_7Profiler@core@audio@rw@@6B@'];vt=b.bases[(oi,sec)]+v;errors=[];rng=random.Random(0xfb0c);addr=F.M.S.SELF;prof=F.M.S.AUX+0x1000
class Stop(Exception):pass
for i in range(128):
 sysimg=bytearray(rng.randbytes(0x1200));img=rng.randbytes(0x100);present=i&1;sysimg[0x34:0x38]=(prof if present else 0).to_bytes(4,'big');both=[]
 for compiled in (False,True):
  c=F.cpu(b);c.mem.wr(addr,sysimg);c.mem.wr(prof,img);F.put(c,prof,vt if compiled else 0x8214b260);c.setg(31,addr);calls=[];stop=sr+0x180 if compiled else 0x82b6fb28
  def observe(pc,word):
   if pc==stop:raise Stop()
  c.instruction_observer=observe
  for at,n in b.unresolved.items():
   if '??3@' in n:c.hooks[at]=lambda x:calls.append('delete')
  c.hooks[0x82815570]=lambda x:calls.append('delete')
  try:c.run(sr+0x164 if compiled else 0x82b6fb0c)
  except Stop:pass
  else:raise AssertionError('shutdown stop not reached')
  state=bytearray(c.mem.rd(prof,0x100));state[:4]=bytes(4);both.append((c.mem.rd(addr,0x1200),state,calls))
 if both[0]!=both[1]:errors.append(i)
r={'cases':128,'mismatches':errors,'objects':{p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in ps},'limits':'Actual original and compiled System::Release profiler call-site region; both null and non-null paths; non-null follows actual virtual Release and deleting destructor with flags zero, vtable addresses normalized, stops before TimerManager release. Not full shutdown or concurrency proof.'};(out/'profiler-lifecycle-shutdown-slice-comparison.json').write_text(json.dumps(r,indent=2)+'\n');print('cases 128 mismatches',errors);assert not errors


