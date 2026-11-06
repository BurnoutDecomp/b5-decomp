from pathlib import Path
import sys,json,random
P=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(P/'scratch/x360_sdk'));import native_atomic_fixture as F
O=Path('D:/Reverse/BP-X360-Experimental-Artifacts/director-526-terra');j=json.loads((O/'vehicle-data-probe/compile-results.json').read_text());obj=Path(j['results'][0]['object']);coff=F.Coff(obj,'VehicleDataConstructProbe');S=F.M.S.SELF;rng=random.Random(0x8221d760)
for i in range(32):
 raw=rng.randbytes(0x160);out=[]
 for compiled in [False,True]:
  c=coff.cpu() if compiled else F.M.new_cpu();c.mem.wr(S,raw);c.setg(3,S);c.run(coff.entry if compiled else 0x8221d760);out.append(c.mem.rd(S,0x160))
 if out[0]!=out[1]:raise AssertionError((i,[hex(x) for x,(a,b) in enumerate(zip(*out)) if a!=b][:40]))
print('passed 32')
