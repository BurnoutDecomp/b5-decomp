from pathlib import Path
import json,random,struct,sys,hashlib
P=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(P/'scratch/x360_sdk'));import native_atomic_fixture as F
O=Path('D:/Reverse/BP-X360-Experimental-Artifacts/camera-three-c0d-terra');j=json.loads((O/'camera-final-v1/compile-results.json').read_text());paths={Path(x['source']).name:Path(x['object']) for x in j['results'] if x['passed']};obj=paths['BrnCameraShakeUpdate.cpp'];rand=paths['CgsRandom.cpp'];sym='?Update@CameraShake@Utils@Camera@BrnDirector@@';b=F.Bundle([F.Coff(obj,sym),F.Coff(rand,'?RandomFloat@Random@CgsNumeric@@QAAMMM'),F.Coff(paths['CameraUtils.cpp'],'?RotateMatrix44AffineByEulerAnglesZXY@Utils@Camera@BrnDirector@@')]);b.select(sym);S=F.M.S.SELF;A=F.M.S.AUX;M=A+0x1000;PARAM=A+0x2000;RNG=A+0x3000;rotaddr=0x821f0000
orig=json.load(open('D:/Reverse/BP-Decomp_Workflow/.ida-exports/BURNOUT_X360_ARTIST.XEX/0x82221310.json'))['assembly'].splitlines()
def target(pc):
 w=F.M.V.x360rd.u32(pc);d=w&0x03fffffc;d=d-0x04000000 if d&0x02000000 else d;return (pc+d)&0xffffffff
for l in orig:
 if '  bl ' in l and 'RotateMatrix44AffineByEulerAnglesZXY' in l:rotaddr=target(int(l.split()[0],16))
def run(compiled,state,matrix,params,rngstate,dt,scale,rotation):
 c=F.cpu(b);c.mem.wr(S,state);c.mem.wr(M,matrix);c.mem.wr(PARAM,params);c.mem.wr(RNG,rngstate);c.setg(3,S);c.setg(4,M);c.setg(5,PARAM);c.setg(6,RNG);c.setf(1,struct.unpack('>f',struct.pack('>f',dt))[0]);c.setf(2,struct.unpack('>f',struct.pack('>f',scale))[0]);trace=[]
 def rotate(cc):trace.append(tuple(cc.vr[1][:3]));cc.mem.wr(cc.g(3),rotation)
 if compiled:
  for at,n in b.unresolved.items():c.hooks[at]=lambda cc,name=n:(_ for _ in ()).throw(AssertionError(name))
  c.run(b.entry)
 else:c.run(0x82221310)
 return c.mem.rd(S,16),c.mem.rd(RNG,0x2c),c.mem.rd(M,64),trace
rng=random.Random(0x82221310)
for i in range(128):
 state=struct.pack('>4f',*[rng.uniform(-2,2) for _ in range(4)]);matrix=struct.pack('>16f',*[rng.uniform(-2,2) for _ in range(16)]);params=struct.pack('>4f',*[rng.uniform(.01,2) for _ in range(4)]);rs=struct.pack('>8fQ I',*[rng.uniform(1,1.99) for _ in range(8)],rng.randrange(2**64),rng.randrange(8));rotation=struct.pack('>16f',*[rng.uniform(-2,2) for _ in range(16)]);args=(state,matrix,params,rs,rng.uniform(.01,.2),rng.uniform(.1,2),rotation);a=run(False,*args);z=run(True,*args)
 if a!=z:
  print('case',i)
  for n,x,y in zip(['state','random','matrix'],a[:3],z[:3]):print(n,[hex(k) for k,(u,v) in enumerate(zip(x,y)) if u!=v][:30],x.hex(),y.hex())
  print('angles',a[3],z[3]);raise AssertionError(i)
r={'cases':128,'scope':'ARTIST 82221310 vs actual compiled CameraShake::Update, real compiled RandomFloat/RandomVector providers vs original inlined random; compare full shake state, random state and transform. Real original and actual compiled rotation providers execute; no rotation mock. Finite inputs, initialized random ring; VMX software model, no hardware claim.','object':str(obj),'sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'random_object':str(rand),'random_sha256':hashlib.sha256(rand.read_bytes()).hexdigest()};(O/'shake-composed-comparison-final-v1.json').write_text(json.dumps(r,indent=2)+'\n');print(json.dumps(r,indent=2))


