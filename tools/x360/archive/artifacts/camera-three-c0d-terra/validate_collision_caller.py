from pathlib import Path
import json,random,struct,sys,hashlib
P=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(P/'scratch/x360_sdk'));import native_atomic_fixture as F
O=Path('D:/Reverse/BP-X360-Experimental-Artifacts/camera-three-c0d-terra');j=json.loads((O/'collision-wip3/compile-results.json').read_text());obj=Path(j['results'][0]['object']);sym='?ProcessSceneQueryResults@FrustrumCollisionResolver@Camera@BrnDirector@@';b=F.Bundle([F.Coff(obj,sym)]);b.select(sym);S=F.M.S.SELF;A=F.M.S.AUX;CAM=A+0x1000;OUT=A+0x3000;INFO=A+0x4000
orig=json.load(open('D:/Reverse/BP-Decomp_Workflow/.ida-exports/BURNOUT_X360_ARTIST.XEX/0x822242F8.json'))['assembly'].splitlines()
def target(pc):
 w=F.M.V.x360rd.u32(pc);d=w&0x03fffffc;d=d-0x04000000 if d&0x02000000 else d;return (pc+d)&0xffffffff
addr=next(target(int(l.split()[0],16)) for l in orig if 'ResolveLineTestNearestUsingDisplacementAndVector' in l and '  bl ' in l)
def run(compiled,box,camera,targetvec):
 c=F.cpu(b);c.mem.wr(S,box);c.mem.wr(CAM,camera);c.mem.wr(OUT,b'\xa5'*16);c.setg(3,S);c.setg(4,INFO);c.setg(5,CAM);c.setg(6,OUT);c.vr[1]=struct.unpack('>4I',targetvec);trace=[]
 def effect(cc,n):
  if 'ResolveLineTestNearestUsingDisplacementAndVector' in n:trace.append((cc.g(3)-S,cc.g(4)-CAM,tuple(cc.vr[1]),tuple(cc.vr[2]),tuple(cc.vr[3])));cc.setg(3,0)
  elif 'GetNearClipDistance' in n:cc.setf(1,struct.unpack('>f',cc.mem.rd(CAM+0x150,4))[0])
  elif n.startswith('__save') or n.startswith('__rest'):pass
  else:raise AssertionError(('unexpected',n))
 if compiled:
  for at,n in b.unresolved.items():c.hooks[at]=lambda cc,name=n:effect(cc,name)
  c.run(b.entry)
 else:c.hooks[addr]=lambda cc:effect(cc,'ResolveLineTestNearestUsingDisplacementAndVector');c.run(0x822242f8)
 return c.g(3),c.mem.rd(OUT,16),c.mem.rd(S,0x160),trace
rng=random.Random(0x822242f8)
for i in range(128):
 box=bytearray(rng.randbytes(0x160));box[0x150:0x154]=struct.pack('>f',rng.uniform(.1,5));camera=bytearray(rng.randbytes(0x170));camera[:64]=struct.pack('>16f',*[rng.uniform(-2,2) for _ in range(16)]);camera[0x58:0x60]=struct.pack('>2f',rng.uniform(30,90),rng.uniform(.7,2));camera[0x150:0x154]=struct.pack('>f',rng.uniform(.1,2));camera[0x15d]=1;vec=struct.pack('>4f',*[rng.uniform(-3,3) for _ in range(4)]);a=run(False,bytes(box),bytes(camera),vec);z=run(True,bytes(box),bytes(camera),vec)
 if a!=z:
  print('case',i,'ret/normal',a[:2],z[:2]);print('traceA',a[3]);print('traceZ',z[3]);print('boxdiff',[hex(k) for k,(u,v) in enumerate(zip(a[2],z[2])) if u!=v]);raise AssertionError(i)
r={'cases':128,'scope':'ARTIST 822242F8 vs actual compiled FrustrumCollisionResolver caller; four controlled resolver calls with all raw v1/v2/v3 lanes and r3/r4 pointers, full resolver object, return and normal output. Finite random camera/target; resolver provider controlled.','object':str(obj),'sha256':hashlib.sha256(obj.read_bytes()).hexdigest()};(O/'collision-caller-comparison-wip.json').write_text(json.dumps(r,indent=2)+'\n');print(json.dumps(r,indent=2))

