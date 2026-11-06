from pathlib import Path
import json,random,struct,sys,hashlib
P=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(P/'scratch/x360_sdk'));import native_atomic_fixture as F
O=Path('D:/Reverse/BP-X360-Experimental-Artifacts/gamestate-gui-c0d-terra');j=json.loads((O/'wip2/compile-results.json').read_text());obj=Path(next(x['object'] for x in j['results'] if x['source'].endswith('BrnGuiCache.cpp')));sym='?UpdateTrackerInfo@GuiCache@BrnGui@@';b=F.Bundle([F.Coff(obj,sym)]);b.select(sym);S=F.M.S.SELF;A=F.M.S.AUX;IND=A+0x1000;TRACK=A+0x2000
orig=json.load(open('D:/Reverse/BP-Decomp_Workflow/.ida-exports/BURNOUT_X360_ARTIST.XEX/0x82506F28.json'))['assembly'].splitlines()
def target(pc):
 w=F.M.V.x360rd.u32(pc);d=w&0x03fffffc;d=d-0x04000000 if d&0x02000000 else d;return (pc+d)&0xffffffff
def run(compiled,indices,lanes):
 c=F.cpu(b);c.mem.wr(S+0x4054,struct.pack('>I',TRACK));c.mem.wr(IND,b''.join(struct.pack('>H',x) for x in indices));c.setg(3,S);c.setg(4,IND);c.setg(5,len(indices));trace=[]
 def effect(cc,n):
  if 'GetLandmarkInfoFromIndex' in n:
   idx=cc.g(4);p=cc.g(5);trace.append(('landmark',idx));cc.mem.wr(p,lanes[idx]+bytes(16)+struct.pack('>H',idx)+bytes(14))
  elif 'RecEvent' in n:
   p=cc.g(4);trace.append(('event',cc.g(3),cc.g(5),cc.g(6),cc.mem.rd(p+0xc00,9),[(cc.mem.rd(p+i*48,4),cc.mem.rd(p+i*48+16,16),cc.mem.rd(p+i*48+40,2)) for i in range(len(indices))]))
  else:raise AssertionError(('unexpected',n))
 if compiled:
  for at,n in b.unresolved.items():c.hooks[at]=lambda cc,name=n:effect(cc,name)
  for n,(oi,v,sec) in b.definitions.items():
   if (oi,sec) in b.bases and any(x in n for x in ['?GetLandmarkInfoFromIndex@GuiCache','?RecEvent@GuiTracker']):c.hooks[b.bases[(oi,sec)]+v]=lambda cc,name=n:effect(cc,name)
  c.run(b.entry)
 else:
  for l in orig:
   if '  bl ' not in l:continue
   p=l.split();n=p[2]
   if n.startswith('__save') or n.startswith('__rest'):continue
   c.hooks[target(int(p[0],16))]=lambda cc,name=n:effect(cc,name)
  c.run(0x82506f28)
 return trace
rng=random.Random(0x82506f28)
for i in range(128):
 indices=list(range(i%9));lanes=[rng.randbytes(16) for _ in range(8)];a=run(False,indices,lanes);z=run(True,indices,lanes)
 if a!=z:print(a,z);raise AssertionError(i)
r={'cases':128,'scope':'Original 82506F28 vs actual GuiCache::UpdateTrackerInfo; count 0..8, raw random 16-byte landmark lanes including W, indices, event header and ordered service calls compared. Landmark lookup and tracker publication controlled; uninitialized unused record bytes and full runtime excluded.','object':str(obj),'sha256':hashlib.sha256(obj.read_bytes()).hexdigest()};(O/'tracker-comparison-wip-wip2.json').write_text(json.dumps(r,indent=2)+'\n');print(json.dumps(r,indent=2))
