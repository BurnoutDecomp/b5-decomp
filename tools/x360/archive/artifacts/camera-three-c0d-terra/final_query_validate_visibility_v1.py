from pathlib import Path
import json,random,struct,sys,hashlib
P=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(P/'scratch/x360_sdk'));import native_atomic_fixture as F
O=Path('D:/Reverse/BP-X360-Experimental-Artifacts/camera-three-c0d-terra');j=json.loads((O/'final-query-v1/compile-results.json').read_text());obj=Path(next(x['object'] for x in j['results'] if x['source'].endswith('BrnVisibilityTest.cpp')));sym='?GenerateSceneQueries@VisibilityTest';b=F.Bundle([F.Coff(obj,sym)]);b.select(sym);S=F.M.S.SELF;A=F.M.S.AUX;CAM=A+0x1000;MAT=A+0x2000;BOX=A+0x3000;REQ=A+0x4000

def run(compiled,state,cam,mat,do,entity,look,zoom):
 c=F.cpu(b);c.mem.wr(S,state);c.mem.wr(CAM,cam);c.mem.wr(MAT,mat);c.mem.wr(BOX,b'\0'*64)
 for r,v in [(3,S),(4,CAM),(5,A),(6,A),(7,REQ),(8,MAT),(9,BOX),(10,do)]:c.setg(r,v)
 c.mem.wr(c.g(1)+0x54,struct.pack('>I',entity));trace=[]
 def effect(cc,n):
  if 'IsLookingAtTarget' in n:cc.setg(3,look)
  elif 'GetZoomFromFOVDegs' in n:cc.setf(1,zoom)
  elif 'LineTestNearest' in n:trace.append((cc.g(3)-REQ,cc.g(4)-S,cc.g(5),cc.g(6),cc.g(7),cc.g(8),tuple(cc.vr[1]),tuple(cc.vr[2])))
  else:raise AssertionError(n)
 if compiled:
  for at,n in b.unresolved.items():c.hooks[at]=lambda cc,name=n:effect(cc,name)
  c.run(b.entry)
 else:
  c.hooks[0x822331f0]=lambda cc:effect(cc,'IsLookingAtTarget');c.hooks[0x821f23e8]=lambda cc:effect(cc,'GetZoomFromFOVDegs');c.hooks[0x82233048]=lambda cc:effect(cc,'LineTestNearest');c.run(0x822400b0)
 return c.mem.rd(S,len(state)),trace
rng=random.Random(0x822400b0)
for i in range(128):
 state=bytearray(rng.randbytes(0xc0));state[0xb0]=rng.randrange(2);cam=bytearray(rng.randbytes(0x170));mat=bytearray(rng.randbytes(64));cam[0x30:0x40]=struct.pack('>4f',*[rng.uniform(-40,40) for _ in range(4)]);mat[0x30:0x40]=struct.pack('>4f',*[rng.uniform(-40,40) for _ in range(4)]);cam[0x58:0x5c]=struct.pack('>f',60.);do=rng.randrange(2);look=rng.randrange(2);zoom=rng.uniform(.2,3);entity=rng.getrandbits(32);a=run(False,bytes(state),bytes(cam),bytes(mat),do,entity,look,zoom);z=run(True,bytes(state),bytes(cam),bytes(mat),do,entity,look,zoom)
 if a!=z:print('case',i,'trace',a[1],z[1],'diff',[hex(k) for k,(x,y) in enumerate(zip(a[0],z[0])) if x!=y]);raise AssertionError(i)
r={'cases':128,'scope':'ARTIST 822400B0 vs actual GenerateSceneQueries; finite randomized vector lanes, look/do gates, controlled look/zoom helpers and Director query calls; full state and two call registers/lanes.','object':str(obj),'sha256':hashlib.sha256(obj.read_bytes()).hexdigest()};(O/'visibility-comparison-final-query-v1.json').write_text(json.dumps(r,indent=2)+'\n');print(json.dumps(r,indent=2))
