from pathlib import Path
import json,random,struct,sys,hashlib
P=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(P/'scratch/x360_sdk'));import native_atomic_fixture as F
O=Path('D:/Reverse/BP-X360-Experimental-Artifacts/gamestate-gui-c0d-terra');j=json.loads((O/'wip1/compile-results.json').read_text());obj=Path(next(x['object'] for x in j['results'] if x['source'].endswith('BrnGuiCache.cpp')));sym='?GetLandmarkInfoAtPositionInList@GuiCache@BrnGui@@';b=F.Bundle([F.Coff(obj,sym)]);b.select(sym);S=F.M.S.SELF;A=F.M.S.AUX;WORLD=A+0x1000;LAND=A+0x2000;OUT=A+0x3000
orig=json.load(open('D:/Reverse/BP-Decomp_Workflow/.ida-exports/BURNOUT_X360_ARTIST.XEX/0x825063C8.json'))['assembly'].splitlines()
def target(pc):
 w=F.M.V.x360rd.u32(pc);d=w&0x03fffffc;d=d-0x04000000 if d&0x02000000 else d;return (pc+d)&0xffffffff
def run(compiled,land,initial,idx):
 c=F.cpu(b);c.mem.wr(S+0x4064,struct.pack('>I',WORLD));c.mem.wr(LAND,land);c.mem.wr(OUT,initial);c.setg(3,S);c.setg(4,idx);c.setg(5,OUT);trace=[]
 def effect(cc,n):
  if 'GetLandmarkInfoAtPositionInList' in n:trace.append(('get',cc.g(3),cc.g(4)));cc.setg(3,LAND)
  elif 'GetDistrict' in n:cc.setg(3,cc.mem.rd(cc.g(3)+0x25,1)[0])
  elif 'DistrictToCounty' in n:trace.append(('county',cc.g(3)));cc.setg(3,2)
  else:raise AssertionError(('unexpected',n))
 if compiled:
  for at,n in b.unresolved.items():c.hooks[at]=lambda cc,name=n:effect(cc,name)
  for n,(oi,v,sec) in b.definitions.items():
   if (oi,sec) in b.bases and any(q in n for q in ['?GetDistrict@SatNavIconInfo','?GetLandmarkInfoAtPositionInList@WorldDataController','?DistrictToCounty@WorldRegion']):c.hooks[b.bases[(oi,sec)]+v]=lambda cc,name=n:effect(cc,name)
  c.run(b.entry)
 else:
  for l in orig:
   if '  bl ' not in l:continue
   p=l.split();n=p[2]
   if n.startswith('__save') or n.startswith('__rest'):continue
   c.hooks[target(int(p[0],16))]=lambda cc,name=n:effect(cc,name)
  c.run(0x825063c8)
 return c.mem.rd(OUT,0x30),trace
rng=random.Random(0x825063c8)
for i in range(128):
 land=bytearray(rng.randbytes(0x40));land[:12]=struct.pack('>3f',*[rng.uniform(-1e5,1e5) for _ in range(3)]);land[0x32]=i%8;args=(bytes(land),rng.randbytes(0x30),i);a=run(False,*args);z=run(True,*args)
 if a!=z:print('trace',a[1],z[1]);print('diff',[hex(k) for k,(x,y) in enumerate(zip(a[0],z[0])) if x!=y]);raise AssertionError(i)
r={'cases':128,'scope':'Original 825063C8 vs actual compiled GuiCache::GetLandmarkInfoAtPositionInList; full 0x30 icon record initialized with poison, XYZ and zero W, ids and metadata, and lookup/county calls checked. Landmark lookup and county mapping controlled; valid objects/districts and finite XYZ.','object':str(obj),'sha256':hashlib.sha256(obj.read_bytes()).hexdigest()};(O/'landmark-comparison-wip.json').write_text(json.dumps(r,indent=2)+'\n');print(json.dumps(r,indent=2))
