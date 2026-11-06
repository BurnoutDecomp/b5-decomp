from pathlib import Path
import sys,json,struct,random,hashlib
P=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(P/'scratch/x360_sdk'));import native_atomic_fixture as F
O=Path('D:/Reverse/BP-X360-Experimental-Artifacts/traffic-plus-abi-terra');j=json.loads((O/'final2/compile-results.json').read_text());obj=Path(next(x['object'] for x in j['results'] if x['source'].endswith('BrnTrafficLightCollection.cpp')));sym='?CalcInstanceTransform@TrafficLightCollection@BrnTraffic@@';b=F.Bundle([F.Coff(obj,sym)]);b.select(sym);A=F.M.S.AUX;COL=A+0x1000;TABLE=A+0x2000;OUT=A+0x3000
def run(compiled,data,idx):
 c=F.cpu(b);c.mem.wr(COL,b'\x00\x08'+bytes(0x12a));c.mem.wr(COL+8,struct.pack('>I',TABLE));c.mem.wr(TABLE,data);c.mem.wr(OUT,b'\xa5'*64);c.setg(3,OUT);c.setg(4,COL);c.setg(5,idx)
 if compiled:
  for at,n in b.unresolved.items():c.hooks[at]=lambda cc,name=n:(_ for _ in ()).throw(AssertionError(name))
  c.run(b.entry)
 else:c.run(0x82753910)
 return c.mem.rd(OUT,64),c.g(3)
rng=random.Random(0x82753910)
for i in range(128):
 vals=[]
 for k in range(8):vals.extend([rng.uniform(-1000,1000),rng.uniform(-1000,1000),rng.uniform(-1000,1000),rng.uniform(-1000,1000)])
 data=struct.pack('>32f',*vals);a=run(False,data,i%8);z=run(True,data,i%8);assert a==z,(i,a[0].hex(),z[0].hex())
r={'cases':128,'scope':'Composed actual original CalcInstanceTransform -> original Expand versus compiled real caller -> compiled real helper, identical r3 sret/record input and full 64-byte output/return. Valid indices and finite inputs; no controlled transform provider.','object':str(obj),'sha256':hashlib.sha256(obj.read_bytes()).hexdigest()};(O/'collection-composed-comparison-final2.json').write_text(json.dumps(r,indent=2)+'\n');print(json.dumps(r,indent=2))
