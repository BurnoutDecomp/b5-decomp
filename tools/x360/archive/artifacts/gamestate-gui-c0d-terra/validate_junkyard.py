from pathlib import Path
import json,random,struct,sys,hashlib,math
P=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(P/'scratch/x360_sdk'));import native_atomic_fixture as F
O=Path('D:/Reverse/BP-X360-Experimental-Artifacts/gamestate-gui-c0d-terra');j=json.loads((O/'wip1/compile-results.json').read_text());obj=Path(next(x['object'] for x in j['results'] if x['source'].endswith('BrnGameStateModule.cpp')));sym='?FindNearestJunkyardID@GameStateModule@BrnGameState@@';b=F.Bundle([F.Coff(obj,sym)]);b.select(sym);S=F.M.S.SELF;A=F.M.S.AUX;DATA=A+0x1000;REGION=A+0x2000
orig=json.load(open('D:/Reverse/BP-Decomp_Workflow/.ida-exports/BURNOUT_X360_ARTIST.XEX/0x8236BAC8.json'))['assembly'].splitlines()
def target(pc):
 w=F.M.V.x360rd.u32(pc);d=w&0x03fffffc;d=d-0x04000000 if d&0x02000000 else d;return (pc+d)&0xffffffff
def run(compiled,pos,regions):
 c=F.cpu(b);c.mem.wr(S+0xab70,struct.pack('>I',DATA));c.mem.wr(DATA+0x44,struct.pack('>II',REGION,len(regions)));c.mem.wr(REGION,b''.join(regions));c.setg(3,S);c.vr[1]=struct.unpack('>4I',pos)
 def effect(cc,n):
  if n.startswith('__save') or n.startswith('__rest'):pass
  elif 'GetMemor' in n:cc.setg(3,DATA)
  elif 'GetGenericRegion' in n:cc.setg(3,REGION+cc.g(4)*56)
  elif n=='sqrt' or n=='sqrtf':cc.setf(1,math.sqrt(cc.f(1)))
  else:raise AssertionError(('unexpected',n))
 if compiled:
  for at,n in b.unresolved.items():c.hooks[at]=lambda cc,name=n:effect(cc,name)
  for n,(oi,v,sec) in b.definitions.items():
   if (oi,sec) in b.bases and '?GetGenericRegion@TriggerData' in n:c.hooks[b.bases[(oi,sec)]+v]=lambda cc,name=n:effect(cc,name)
  c.run(b.entry)
 else:
  for l in orig:
   if '  bl ' not in l:continue
   p=l.split();n=p[2]
   if n.startswith('__save') or n.startswith('__rest'):continue
   c.hooks[target(int(p[0],16))]=lambda cc,name=n:effect(cc,name)
  c.run(0x8236bac8)
 return c.g(3)
rng=random.Random(0x8236bac8)
for i in range(128):
 xyz=[float(rng.randrange(-50,50)) for _ in range(3)];pos=struct.pack('>4f',*xyz,float(rng.uniform(-1000,1000)));regions=[];winner=i%3
 for k in range(3):
  delta=1 if k==winner else 10+10*k;axis=(i+k)%3;loc=xyz[:];loc[axis]+=delta
  z=bytearray(56);z[:12]=struct.pack('>3f',*loc);z[36:40]=struct.pack('>I',0x100+k);z[54]=1 if k==(winner+1)%3 else 0;regions.append(bytes(z))
 a=run(False,pos,regions);z=run(True,pos,regions);assert a==z==0x100+winner,(i,a,z)
r={'cases':128,'scope':'Actual ARTIST vs compiled FindNearestJunkyardID; input VPU Vector3 in v1, all four lanes varied, three regions and nearest chosen ID compared. Region accessor and scalar sqrt controlled; integer axis-aligned X/Y/Z distances with varying winner deliberately avoid FP/tie edge cases. Full numeric parity not claimed.','object':str(obj),'sha256':hashlib.sha256(obj.read_bytes()).hexdigest()};(O/'junkyard-comparison-wip.json').write_text(json.dumps(r,indent=2)+'\n');print(json.dumps(r,indent=2))

