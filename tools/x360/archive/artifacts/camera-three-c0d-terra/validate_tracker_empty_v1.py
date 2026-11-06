from pathlib import Path
import json,random,struct,sys,hashlib
P=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(P/'scratch/x360_sdk'));import native_atomic_fixture as F
O=Path('D:/Reverse/BP-X360-Experimental-Artifacts/camera-three-c0d-terra');j=json.loads((O/'gyro-v2/compile-results.json').read_text());obj=Path(j['results'][0]['object']);sym='?GetImplicitVelocity@VehicleTracker';b=F.Bundle([F.Coff(obj,sym)]);b.select(sym);S=F.M.S.SELF;OUT=F.M.S.AUX+0x1000

def run(compiled,state,out):
 c=F.cpu(b);c.mem.wr(S,state);c.mem.wr(OUT,out);c.setg(3,OUT);c.setg(4,S)
 if compiled:
  for at,n in b.unresolved.items():c.hooks[at]=lambda cc,name=n:(_ for _ in ()).throw(AssertionError(name))
  c.run(b.entry)
 else:c.run(0x82205f70)
 return c.g(3),c.mem.rd(OUT,16),c.mem.rd(S,len(state))
rng=random.Random(0x82205f70)
for i in range(128):
 state=bytearray(rng.randbytes(0x2b0));state[0x84:0x88]=struct.pack('>I',i%2);out=rng.randbytes(16);a=run(False,bytes(state),out);z=run(True,bytes(state),out)
 if a!=z:raise AssertionError(i)
r={'cases':128,'scope':'Original 82205F70 vs actual header GetImplicitVelocity: 0/1 sample zero path, hidden return r3/this r4, all 16 output bytes, return pointer and full tracker state with poison.','object':str(obj),'sha256':hashlib.sha256(obj.read_bytes()).hexdigest()};(O/'tracker-empty-velocity-comparison-v1.json').write_text(json.dumps(r,indent=2)+'\n');print(json.dumps(r,indent=2))
