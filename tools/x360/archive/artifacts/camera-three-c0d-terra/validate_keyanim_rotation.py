from pathlib import Path
import json,random,struct,sys,hashlib
P=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(P/'scratch/x360_sdk'));import native_atomic_fixture as F
O=Path('D:/Reverse/BP-X360-Experimental-Artifacts/camera-three-c0d-terra');j=json.loads((O/'wip3/compile-results.json').read_text());obj=Path(next(x['object'] for x in j['results'] if x['source'].endswith('BrnKeyAnimShakeController.cpp')));sym='?Update@KeyAnimShakeController@BrnDirector@@';b=F.Bundle([F.Coff(obj,sym)]);b.select(sym);A=F.M.S.AUX;TAKE=A+0x1000;CAM=A+0x2000;effects=CAM+0x68
class Stop(Exception):pass
def run(compiled,key,values,amp_before,amp_after):
 c=F.cpu(b);c.mem.wr(CAM+0x114,struct.pack('>f',amp_before));trace=[]
 def sample(cc):
  assert cc.g(3)==TAKE and cc.g(5)==key;index=cc.g(4);trace.append(index);cc.setf(1,values[index]);
  if len(trace)==3:cc.mem.wr(CAM+0x114,struct.pack('>f',amp_after))
 if compiled:
  c.setg(30,TAKE);c.setg(31,key);c.setg(28,effects);c.setg(27,0)
  for at,n in b.unresolved.items():
   if '?GetValueFloat@ICETake' in n:c.hooks[at]=sample
   else:c.hooks[at]=lambda cc,name=n:(_ for _ in ()).throw(AssertionError(name))
  entry=b.entry+0x1c0;end=b.entry+0x248;vreg=13
 else:
  c.setg(29,TAKE);c.setg(30,key);c.setg(31,CAM);c.setg(28,0);c.hooks[0x8252f848]=sample;entry=0x8223d654;end=0x8223d6f0;vreg=0
 def observe(pc,w):
  if pc==end:raise Stop()
 c.instruction_observer=observe
 try:c.run(entry)
 except Stop:pass
 return tuple(c.vr[vreg]),trace
rng=random.Random(0x8223d654)
for i in range(128):
 values={k:struct.unpack('>f',struct.pack('>f',rng.uniform(-4,4)))[0] for k in (22,23,24)};args=(rng.randrange(65536),values,0.25,struct.unpack('>f',struct.pack('>f',rng.uniform(-4,4)))[0]);a=run(False,*args);z=run(True,*args)
 if a!=z:print(i,a,z);raise AssertionError(i)
r={'cases':128,'scope':'Original 8223D654..D6EC vs actual compiled Update slice: exact sampled element order Z,Y,X, key and full four-lane scaled rotation vector. ICE sampler controlled and mutates camera amplitude on third sample; both implementations use updated amplitude. Finite floats, no quaternion/whole update claim.','object':str(obj),'sha256':hashlib.sha256(obj.read_bytes()).hexdigest()};(O/'keyanim-rotation-comparison-wip.json').write_text(json.dumps(r,indent=2)+'\n');print(json.dumps(r,indent=2))
