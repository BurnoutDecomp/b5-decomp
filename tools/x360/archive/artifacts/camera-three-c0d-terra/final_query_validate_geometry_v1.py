from pathlib import Path
import json,random,struct,sys,hashlib
P=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(P/'scratch/x360_sdk'));import native_atomic_fixture as F
O=Path('D:/Reverse/BP-X360-Experimental-Artifacts/camera-three-c0d-terra');j=json.loads((O/'final-query-v1/compile-results.json').read_text());obj=Path(next(x['object'] for x in j['results'] if x['source'].endswith('BrnGeometryCollisionPredictor.cpp')));sym='?GenerateSceneQueries@GeometryCollisionPredictor';b=F.Bundle([F.Coff(obj,sym)]);b.select(sym);S=F.M.S.SELF;A=F.M.S.AUX;CAM=A+0x1000;REQ=A+0x2000;HOLDER=A+0x3000;CODE=A+0x5000

def run(compiled,velocity,start):
 c=F.cpu(b);c.mem.wr(S+0x80,velocity+b'\xa5'*0x70);c.mem.wr(S+0x220,velocity);c.mem.wr(CAM+0x30,start);c.mem.wr(HOLDER+8,struct.pack('>I',REQ));trace=[]
 def record(cc):trace.append((cc.g(3)-REQ,cc.g(4)-(S+0x80),cc.g(5),cc.g(6),cc.g(7)&0xffffffff,cc.g(8),tuple(cc.vr[1]),tuple(cc.vr[2])))
 if compiled:
  c.mem.add(0x83800030,0x10);c.mem.wr(0x83800030,b'\0'*8+struct.pack('>f',1.)+b'\0'*4)
  c.setg(3,S+0x80);c.setg(4,CAM);c.setg(7,REQ)
  for at,n in b.unresolved.items():c.hooks[at]=record
  c.run(b.entry)
 else:
  c.mem.add(0x82faa000,0x1000);c.mem.wr(0x82faa9a0,struct.pack('>4f',1,1,1,1));c.mem.add(CODE,0x1000);data=b''.join(struct.pack('>I',F.M.V.x360rd.u32(pc)) for pc in range(0x82240468,0x822404ac,4))+struct.pack('>I',0x4e800020);c.mem.wr(CODE,data);c.setg(31,S);c.setg(26,CAM);c.setg(30,HOLDER);c.setg(28,0x82ce0000);c.run(CODE);record(c)
 return trace
rng=random.Random(0x82240468)
for i in range(128):
 vel=struct.pack('>4f',*[rng.uniform(-30,30) for _ in range(4)]);pos=struct.pack('>4f',*[rng.uniform(-30,30) for _ in range(4)]);a=run(False,vel,pos);z=run(True,vel,pos)
 if a!=z:print(i,a,z);raise AssertionError(i)
r={'cases':128,'scope':'ARTIST 82240468..A8 inlined predictor vs actual compiled GeometryCollisionPredictor GenerateSceneQueries; all query scalar registers and raw v1/v2 lanes, finite randomized velocity/start, query call controlled.','object':str(obj),'sha256':hashlib.sha256(obj.read_bytes()).hexdigest()};(O/'geometry-comparison-final-query-v1.json').write_text(json.dumps(r,indent=2)+'\n');print(json.dumps(r,indent=2))
