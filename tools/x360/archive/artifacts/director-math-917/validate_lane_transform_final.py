from pathlib import Path
import sys,json,random,struct,hashlib
P=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(P/'scratch/x360_sdk'));import native_atomic_fixture as F
O=Path('D:/Reverse/BP-X360-Experimental-Artifacts/director-math-917');j=json.loads((O/'final/compile-results.json').read_text());obj=Path(next(x['object'] for x in j['results'] if x['source'].endswith('BrnBehaviourRoadRunner.cpp')));sym='?CalcTransformFromLanePosition@TrafficLaneTruck@';b=F.Bundle([F.Coff(obj,sym)]);b.select(sym);S=F.M.S.SELF;A=F.M.S.AUX; WORLD=A+0x1000;DATA=A+0x2000;TABLE=A+0x3000;HULL=A+0x4000;SECT=A+0x5000;RUNGS=A+0x6000
orig=json.load(open('D:/Reverse/BP-Decomp_Workflow/.ida-exports/BURNOUT_X360_ARTIST.XEX/0x8222A640.json'))['assembly'].splitlines()
def target(pc):
 w=F.M.V.x360rd.u32(pc);d=w&0x03fffffc;d=d-0x04000000 if d&0x02000000 else d;return (pc+d)&0xffffffff
def put(c,a,x):c.mem.wr(a,struct.pack('>I',x))
def run(compiled,initial,vec,mat):
 c=F.cpu(b);c.mem.wr(S,initial);put(c,WORLD+0xf8,6);put(c,DATA+12,TABLE);put(c,TABLE,HULL);put(c,HULL+20,RUNGS);c.setg(3,S);c.setg(4,WORLD);t=[]
 def effect(cc,n):
  if 'GetTrafficData' in n or 'TrafficData___operator' in n:cc.setg(3,DATA)
  elif 'GetHull' in n:cc.setg(3,HULL)
  elif 'GetSection' in n:cc.setg(3,SECT)
  elif 'CalcDirectionAtParameter' in n:t.append(('direction',cc.g(3),cc.g(4),cc.g(5),tuple(cc.vr[1])));cc.mem.wr(cc.g(6),vec)
  elif 'CreateLookAt' in n:t.append(('lookat',tuple(cc.vr[1]),tuple(cc.vr[2])));cc.mem.wr(cc.g(3),mat)
  else:raise AssertionError(('unknown',n))
 if compiled:
  for at,n in b.unresolved.items():c.hooks[at]=lambda cc,name=n:effect(cc,name)
  for n,(oi,v,sec) in b.definitions.items():
   if (oi,sec) in b.bases and any(x in n for x in ['?GetTrafficData@WorldMap','?GetHull@TrafficData','?GetSection@Hull','?CalcDirectionAtParameter@Section','?CreateLookAt@Utils']):c.hooks[b.bases[(oi,sec)]+v]=lambda cc,name=n:effect(cc,name)
  c.run(b.entry)
 else:
  for l in orig:
   if '  bl ' not in l:continue
   p=l.split();n=p[2]
   if n.startswith('__save') or n.startswith('__rest'):continue
   c.hooks[target(int(p[0],16))]=lambda cc,name=n:effect(cc,name)
  c.run(0x8222a640)
 return c.mem.rd(S,0x90),t
rng=random.Random(0x8222a640)
for i in range(128):
 x=bytearray(rng.randbytes(0x90));x[:16]=struct.pack('>4f',*[rng.uniform(-100,100) for _ in range(4)]);x[20:24]=struct.pack('>f',rng.uniform(-100,100));x[24:28]=bytes(4);x[28]=0;x[29]=rng.randrange(256);direction=struct.pack('>4f',*[rng.uniform(-100,100) for _ in range(4)]);args=(bytes(x),direction,rng.randbytes(64));a=run(False,*args);z=run(True,*args)
 if a!=z:print(a[1],z[1]);print([hex(k) for k,(v,w) in enumerate(zip(a[0],z[0])) if v!=w]);raise AssertionError(i)
r={'cases':128,'scope':'ARTIST 8222A640 vs actual compiled CalcTransformFromLanePosition; full truck memory, all four broadcast parameter lanes, both look-at vectors and resulting matrix compared. World/traffic lookup, lane direction and look-at providers controlled. Input vector lanes use finite randomized floats.','object':str(obj),'sha256':hashlib.sha256(obj.read_bytes()).hexdigest()};(O/'lane-transform-comparison-wip-final.json').write_text(json.dumps(r,indent=2)+'\n');print(json.dumps(r,indent=2))

