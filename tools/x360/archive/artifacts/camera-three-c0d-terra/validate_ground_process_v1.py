from pathlib import Path
import json,random,struct,sys,hashlib
P=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(P/'scratch/x360_sdk'));import native_atomic_fixture as F
O=Path('D:/Reverse/BP-X360-Experimental-Artifacts/camera-three-c0d-terra');j=json.loads((O/'visibility-policy-v1/compile-results.json').read_text());obj=Path(j['results'][0]['object']);sym='?ProcessSceneQueryResults@GroundConstraint';b=F.Bundle([F.Coff(obj,sym)]);b.select(sym);S=F.M.S.SELF;CAM=F.M.S.AUX+0x1000

def run(compiled,state,cam):
 c=F.cpu(b);c.mem.wr(S,state);c.mem.wr(CAM,cam);c.setg(3,S);c.setg(5,CAM)
 if compiled:
  for at,n in b.unresolved.items():c.hooks[at]=lambda cc,name=n:(_ for _ in ()).throw(AssertionError(name))
  c.run(b.entry)
 else:c.run(0x8220e3a0)
 return c.g(3),c.mem.rd(S,len(state)),c.mem.rd(CAM,len(cam))
rng=random.Random(0x8220e3a0)
for i in range(128):
 state=bytearray(rng.randbytes(0x60));state[:4]=struct.pack('>I',2);state[0x10:0x20]=struct.pack('>4f',*[rng.uniform(-20,20) for _ in range(4)]);state[0x48]=rng.randrange(2);state[0x50:0x54]=struct.pack('>f',rng.uniform(0,12));cam=bytearray(rng.randbytes(0x170));cam[0x30:0x40]=struct.pack('>4f',*[rng.uniform(-20,20) for _ in range(4)]);a=run(False,bytes(state),bytes(cam));z=run(True,bytes(state),bytes(cam))
 if a!=z:print('case',i,'ret',a[0],z[0],'objdiff',[hex(k) for k,(x,y) in enumerate(zip(a[1],z[1])) if x!=y],'camdiff',[hex(k) for k,(x,y) in enumerate(zip(a[2],z[2])) if x!=y]);raise AssertionError(i)
r={'cases':128,'scope':'ARTIST 8220E3A0 vs actual GroundConstraint ProcessSceneQueryResults; valid postbox hit/miss, finite nonnegative height and randomized four-lane camera/hit vectors; complete object/camera bytes and return.','object':str(obj),'sha256':hashlib.sha256(obj.read_bytes()).hexdigest()};(O/'ground-process-comparison-v1.json').write_text(json.dumps(r,indent=2)+'\n');print(json.dumps(r,indent=2))
