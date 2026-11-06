from pathlib import Path
import json,random,struct,sys,hashlib
P=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(P/'scratch/x360_sdk'));import native_atomic_fixture as F
O=Path('D:/Reverse/BP-X360-Experimental-Artifacts/camera-three-c0d-terra');j=json.loads((O/'visibility-policy-v1/compile-results.json').read_text());obj=Path(j['results'][0]['object']);sym='?GenerateSceneQueries@GroundConstraint';b=F.Bundle([F.Coff(obj,sym)]);b.select(sym);S=F.M.S.SELF;A=F.M.S.AUX;CAM=A+0x1000;REQ=A+0x2000

def run(compiled,state,cam):
 c=F.cpu(b);c.mem.wr(S,state);c.mem.wr(CAM,cam);c.setg(3,S);c.setg(4,CAM);c.setg(6,REQ);trace=[]
 def hook(cc,n):
  if 'LineTestNearest' in n:trace.append((cc.g(3)-REQ,cc.g(4)-S,cc.g(5),cc.g(6),cc.g(7)&0xffffffff,cc.g(8),tuple(cc.vr[1]),tuple(cc.vr[2])))
  else:raise AssertionError(n)
 if compiled:
  c.mem.add(0x83800040,0x10);c.mem.wr(0x83800040,struct.pack('>f',1.)+b'\0'*12)
  for at,n in b.unresolved.items():c.hooks[at]=lambda cc,name=n:hook(cc,name)
  c.run(b.entry)
 else:c.hooks[0x82233048]=lambda cc:hook(cc,'LineTestNearest');c.run(0x82240200)
 return trace,c.mem.rd(S,len(state))
rng=random.Random(0x82240200)
for i in range(128):
 state=bytearray(rng.randbytes(0x60));state[0x50:0x54]=struct.pack('>f',rng.uniform(0,12));cam=bytearray(rng.randbytes(0x170));cam[0x30:0x40]=struct.pack('>4f',*[rng.uniform(-20,20) for _ in range(4)]);a=run(False,bytes(state),bytes(cam));z=run(True,bytes(state),bytes(cam))
 if a!=z:print(i,a[0],z[0]);raise AssertionError(i)
r={'cases':128,'scope':'ARTIST 82240200 vs actual GroundConstraint GenerateSceneQueries; finite nonnegative heights and full four-lane camera vectors; query controlled; compares full object, scalar registers and vector lanes.','object':str(obj),'sha256':hashlib.sha256(obj.read_bytes()).hexdigest()};(O/'ground-query-comparison-v1.json').write_text(json.dumps(r,indent=2)+'\n');print(json.dumps(r,indent=2))
