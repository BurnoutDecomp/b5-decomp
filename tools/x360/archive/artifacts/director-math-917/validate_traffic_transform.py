from pathlib import Path
import json,sys,random,struct,hashlib,math
P=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(P/'scratch/x360_sdk'));import native_atomic_fixture as F
O=Path('D:/Reverse/BP-X360-Experimental-Artifacts/director-math-917');j=json.loads((O/'wip2/compile-results.json').read_text());obj=Path(next(x['object'] for x in j['results'] if x['source'].endswith('BrnTrafficLightCollection.cpp')));sym='?ExpandPosPlusYRotToTransform@BrnTraffic@@';b=F.Bundle([F.Coff(obj,sym)]);b.select(sym);S=F.M.S.SELF;A=F.M.S.AUX;IN=A+0x1000;OUT=A+0x2000;rng=random.Random(0x823610b8)
def run(compiled,data):
 c=F.cpu(b);c.mem.wr(IN,data);c.mem.wr(OUT,bytes([0xa5])*64);c.setg(3,OUT)
 if compiled:
  c.setg(4,IN)
  for at,n in b.unresolved.items():
   if n=='floor':c.hooks[at]=lambda cc:cc.setf(1,float(math.floor(cc.f(1))))
   else:c.hooks[at]=lambda cc,name=n:(_ for _ in ()).throw(AssertionError(name))
  c.run(b.entry)
 else:c.vr[1]=struct.unpack('>4I',data);c.run(0x823610b8)
 return c.mem.rd(OUT,64)
for i in range(64):
 data=struct.pack('>4f',*[rng.uniform(-1000,1000) for _ in range(3)],0.0);a=run(False,data);z=run(True,data)
 if a!=z:print('diff',[hex(k) for k,(x,y) in enumerate(zip(a,z)) if x!=y]);print(a.hex(),z.hex());raise AssertionError(i)
r={'cases':64,'scope':'ARTIST 823610B8 versus actual compiled ExpandPosPlusYRotToTransform with arbitrary finite XYZ and zero rotation, comparing complete 64-byte returned matrix. Original input delivered in v1; current candidate input via r4 pointer (known ABI mismatch); CRT floor controlled. Nonzero angles and full ABI parity not covered.','object':str(obj),'sha256':hashlib.sha256(obj.read_bytes()).hexdigest()};(O/'traffic-transform-zero-comparison.json').write_text(json.dumps(r,indent=2)+'\n');print(json.dumps(r,indent=2))
