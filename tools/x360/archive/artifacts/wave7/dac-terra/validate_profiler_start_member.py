import sys,json,random,hashlib
from pathlib import Path
root=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(root/'scratch/x360_sdk'));import native_atomic_fixture as F
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave7/dac-terra');p=out/'profiler-native-build/Profiler.obj';b=F.Bundle([F.Coff(p,'?Start@Profiler@')]);b.select('?Start@Profiler@');assert F.M.V.x360rd.u32(0x82815570)==0x7c8a2378
addr=F.M.S.SELF;errors=[];rng=random.Random(0x826a9)
for i in range(128):
 initial=rng.randbytes(0x100);cycle=rng.getrandbits(32);results=[]
 for compiled in (False,True):
  c=F.cpu(b);c.mem.wr(addr,initial);c.setg(3,addr);calls=[]
  def sample(cpu):calls.append('GetCpuCycle');cpu.setg(3,cycle)
  c.hooks[0x82b64558]=sample
  for at,name in b.unresolved.items():
   if 'GetCpuCycle' in name:c.hooks[at]=sample
  c.run(b.entry if compiled else 0x82b6a9c8);results.append((c.mem.rd(addr,0x100),calls,c.g(3)&0xffffffff))
 if results[0]!=results[1]:errors.append(i)
r={'cases':128,'mismatches':errors,'original':'82B6A9C8','object':str(p),'sha256':hashlib.sha256(p.read_bytes()).hexdigest(),'limits':'Actual compiled Start vs ARTIST, GetCpuCycle controlled identically; return, call and full object/guard bytes compared. No hardware timing or concurrency claim.'};(out/'profiler-start-member-comparison.json').write_text(json.dumps(r,indent=2)+'\n');print('cases 128 mismatches',errors);assert not errors
