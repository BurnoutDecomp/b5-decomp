from pathlib import Path
import sys,json,random,struct,hashlib
P=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(P/'scratch/x360_sdk'));import native_atomic_fixture as F
O=Path('D:/Reverse/BP-X360-Experimental-Artifacts/traffic-plus-abi-terra');j=json.loads((O/'final/compile-results.json').read_text());obj=Path(next(x['object'] for x in j['results'] if x['source'].endswith('BrnBehaviourRoadRunner.cpp')));sym='?Prepare@TrafficLaneTruck@Camera@BrnDirector@@';b=F.Bundle([F.Coff(obj,sym)]);b.select(sym);S=F.M.S.SELF;A=F.M.S.AUX;orig=json.load(open('D:/Reverse/BP-Decomp_Workflow/.ida-exports/BURNOUT_X360_ARTIST.XEX/0x82247A08.json'))['assembly'].splitlines()
def target(pc):
 w=F.M.V.x360rd.u32(pc);d=w&0x03fffffc;d=d-0x04000000 if d&0x02000000 else d;return (pc+d)&0xffffffff
def run(compiled,initial,lane,vec):
 c=F.cpu(b);c.mem.wr(S,initial);c.setg(3,S);c.setg(4,A+0x3000);c.vr[1]=vec;t=[]
 def effect(cc,n):
  if 'GetLanePositionNearestPoint' in n:
   t.append(('lane',cc.g(4),tuple(cc.vr[1])));cc.mem.wr(cc.g(3),lane)
  elif 'CalcTransformFromLanePosition' in n:t.append(('transform',cc.g(3),cc.g(4)))
  else:raise AssertionError(n)
 if compiled:
  for at,n in b.unresolved.items():c.hooks[at]=lambda cc,name=n:effect(cc,name)
  for n,(oi,v,sec) in b.definitions.items():
   if (oi,sec) in b.bases and '?CalcTransformFromLanePosition@TrafficLaneTruck' in n:c.hooks[b.bases[(oi,sec)]+v]=lambda cc,name=n:effect(cc,name)
  c.run(b.entry)
 else:
  for l in orig:
   if '  bl ' not in l:continue
   p=l.split();c.hooks[target(int(p[0],16))]=lambda cc,name=p[2]:effect(cc,name)
  c.run(0x82247a08)
 return c.mem.rd(S,0x90),c.g(3),t
rng=random.Random(0x82247a08);count=0
for valid in (0,1):
 for i in range(128):
  init=rng.randbytes(0x90);lane=bytearray(rng.randbytes(32));lane[30]=valid;vec=struct.unpack('>4I',rng.randbytes(16));args=(init,bytes(lane),vec);x=run(False,*args);y=run(True,*args)
  if x!=y:print(x[1:],y[1:]);print([hex(i) for i,(a,z) in enumerate(zip(x[0],y[0])) if a!=z]);raise AssertionError((valid,i))
  count+=1
r={'cases':count,'scope':'Actual ARTIST and compiled TrafficLaneTruck::Prepare, full 0x90 object, return, point v1 lanes and ordered service arguments. Nearest lane query and lane transform are controlled; both valid and invalid branches tested with random poisoned state.','object':str(obj),'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest()};(O/'prepare-comparison-wip-final.json').write_text(json.dumps(r,indent=2)+'\n');print(json.dumps(r,indent=2))
