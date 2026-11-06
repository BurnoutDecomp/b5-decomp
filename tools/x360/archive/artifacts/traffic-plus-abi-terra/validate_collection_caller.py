from pathlib import Path
import sys,json,struct,random,hashlib
P=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(P/'scratch/x360_sdk'));import native_atomic_fixture as F
O=Path('D:/Reverse/BP-X360-Experimental-Artifacts/traffic-plus-abi-terra');j=json.loads((O/'final/compile-results.json').read_text());obj=Path(next(x['object'] for x in j['results'] if x['source'].endswith('BrnTrafficLightCollection.cpp')));sym='?CalcInstanceTransform@TrafficLightCollection@BrnTraffic@@';b=F.Bundle([F.Coff(obj,sym)]);b.select(sym);A=F.M.S.AUX;COL=A+0x1000;TABLE=A+0x2000;OUT=A+0x3000
orig=json.load(open('D:/Reverse/BP-Decomp_Workflow/.ida-exports/BURNOUT_X360_ARTIST.XEX/0x82753910.json'))['assembly'].splitlines()
def target(pc):
 w=F.M.V.x360rd.u32(pc);d=w&0x03fffffc;d=d-0x04000000 if d&0x02000000 else d;return (pc+d)&0xffffffff
def run(compiled,data,idx,result):
 c=F.cpu(b);c.mem.wr(COL,b'\x00\x08'+bytes(0x12a));c.mem.wr(COL+8,struct.pack('>I',TABLE));c.mem.wr(TABLE,data);c.mem.wr(OUT,b'\xa5'*64);c.setg(3,OUT);c.setg(4,COL);c.setg(5,idx);trace=[]
 def effect(cc,n):
  if 'ExpandPosPlusYRotToTransform' in n:trace.append((cc.g(3),tuple(cc.vr[1])));cc.mem.wr(cc.g(3),result);cc.setg(3,cc.g(3))
  else:raise AssertionError(n)
 if compiled:
  for at,n in b.unresolved.items():c.hooks[at]=lambda cc,name=n:effect(cc,name)
  for n,(oi,v,sec) in b.definitions.items():
   if (oi,sec) in b.bases and '?ExpandPosPlusYRotToTransform@BrnTraffic' in n:c.hooks[b.bases[(oi,sec)]+v]=lambda cc,name=n:effect(cc,name)
  c.run(b.entry)
 else:
  for l in orig:
   if '  bl ' not in l:continue
   p=l.split();n=p[2]
   if n.startswith('__save') or n.startswith('__rest'):continue
   c.hooks[target(int(p[0],16))]=lambda cc,name=n:effect(cc,name)
  c.run(0x82753910)
 return c.mem.rd(OUT,64),c.g(3),trace
rng=random.Random(0x82753910)
for i in range(128):
 args=(rng.randbytes(16*8),i%8,rng.randbytes(64));a=run(False,*args);z=run(True,*args)
 assert a==z,(i,a,z)
r={'cases':128,'scope':'Original ARTIST CalcInstanceTransform versus compiled actual method; full raw input v1 lanes, sret r3, return pointer and 64 output bytes. Transform helper is controlled and valid indices used.','object':str(obj),'sha256':hashlib.sha256(obj.read_bytes()).hexdigest()};(O/'collection-caller-comparison.json').write_text(json.dumps(r,indent=2)+'\n');print(json.dumps(r,indent=2))
