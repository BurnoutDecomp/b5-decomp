from pathlib import Path
import sys,random,struct,hashlib,json
P=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(P/'scratch/x360_sdk'));import native_atomic_fixture as F
O=Path('D:/Reverse/BP-X360-Experimental-Artifacts/traffic-plus-abi-terra');j=json.loads((O/'final/compile-results.json').read_text());obj=next(Path(x['object']) for x in j['results'] if x['source'].endswith('spaces_probe.cpp'));sym='?DirectorPlayerSpacesProbe@@';b=F.Bundle([F.Coff(obj,sym)]);b.select(sym);A=F.M.S.AUX; VEH=A+0x1000;SELF=F.M.S.SELF
class Stop(Exception):pass
def run(compiled,pos,forward):
 c=F.cpu(b);c.mem.wr(VEH+0x210,forward);c.mem.wr(VEH+0x220,pos);c.mem.wr(VEH+0x44a,b'\1');seen=[]
 def effect(cc,n):
  if 'GetPlayer@AllVehicleData' in n:cc.setg(3,VEH)
  elif 'CreateLookAt' in n:seen.append((tuple(cc.vr[1]),tuple(cc.vr[2])));raise Stop()
  else:raise AssertionError(n)
 if compiled:
  c.setg(3,SELF)
  for at,n in b.unresolved.items():c.hooks[at]=lambda cc,name=n:effect(cc,name)
  for n,(oi,v,sec) in b.definitions.items():
   if (oi,sec) in b.bases and '?GetPlayer@AllVehicleData' in n:c.hooks[b.bases[(oi,sec)]+v]=lambda cc,name=n:effect(cc,name)
  entry=b.entry
 else:
  c.setg(3,VEH);pc=0x8221dd18;w=F.M.V.x360rd.u32(pc);d=w&0x03fffffc;d=d-0x04000000 if d&0x02000000 else d;c.hooks[(pc+d)&0xffffffff]=lambda cc:effect(cc,'CreateLookAt');entry=0x8221dcd8
 try:c.run(entry)
 except Stop:pass
 assert len(seen)==1
 return seen[0]
rng=random.Random(0x8221dd18);negative=0
for i in range(128):
 pos=struct.pack('>4f',*[rng.uniform(-100,100) for _ in range(4)]);forward=struct.pack('>4f',*[rng.uniform(-100,100) for _ in range(4)]);a=run(False,pos,forward);z=run(True,pos,forward);assert a==z,(i,a,z);negative+=int(a[1][3]!=0)
assert negative==128
r={'cases':128,'negative_control_detected':negative,'scope':'Actual original ARTIST 8221DCD8..DD18 versus actual-header compiled UpdatePlayerSpaces up to the CreateLookAt boundary. Compare both complete register vectors, including W, across random finite lanes; current player getter controlled, crashing branch skips impact-space work. A zero-W substitute fails all 128 cases.','object':str(obj),'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest()};(O/'spaces-heading-comparison-final.json').write_text(json.dumps(r,indent=2)+'\n');print(json.dumps(r,indent=2))
