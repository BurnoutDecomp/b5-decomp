from pathlib import Path
import json,random,sys,hashlib
P=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(P/'scratch/x360_sdk'));import native_atomic_fixture as F
O=Path('D:/Reverse/BP-X360-Experimental-Artifacts/spiralling-r2-terra');j=json.loads((O/'representative-v1/compile-results.json').read_text());obj=Path(j['results'][0]['object']);sym='?Construct@BehaviourSpirallingDeathcam@';b=F.Bundle([F.Coff(obj,sym)]);b.select(sym);S=F.M.S.SELF

def run(compiled,state):
 c=F.cpu(b);c.mem.wr(S,state);c.setg(3,S)
 if compiled:
  for at,n in b.unresolved.items():c.hooks[at]=lambda cc:None
  c.run(b.entry)
 else:c.hooks[0x82224890]=lambda cc:None;c.run(0x8222c088)
 return c.mem.rd(S+0x270,0x30)
rng=random.Random(0x8222c088)
for i in range(256):
 state=rng.randbytes(0x300);a=run(False,state);z=run(True,state)
 if a!=z:raise AssertionError((i,a.hex(),z.hex()))
r={'cases':256,'scope':'ARTIST 8222C088 vs actual whole Construct; poisoned full orbit vector data and following attachment position (48 bytes) matched, original collision and actual base/vehicle-ref providers controlled; no full lifecycle claim.','object':str(obj),'sha256':hashlib.sha256(obj.read_bytes()).hexdigest()};(O/'construct-vector-comparison-v1.json').write_text(json.dumps(r,indent=2)+'\n');print(json.dumps(r,indent=2))
