from pathlib import Path
import json,random,struct,sys,hashlib
P=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(P/'scratch/x360_sdk'));import native_atomic_fixture as F
O=Path('D:/Reverse/BP-X360-Experimental-Artifacts/camera-three-c0d-terra');j=json.loads((O/'utils-wip6/compile-results.json').read_text());obj=Path(j['results'][0]['object']);sym='?RotateMatrix44AffineByEulerAnglesZXY@Utils@Camera@BrnDirector@@';b=F.Bundle([F.Coff(obj,sym)]);b.select(sym);S=F.M.S.SELF;rng=random.Random(0x82204f98)
def run(compiled,mat,ang):
 c=F.cpu(b);c.mem.wr(S,mat);c.setg(3,S);c.vr[1]=struct.unpack('>4I',ang)
 for at,n in b.unresolved.items():c.hooks[at]=lambda cc,name=n:(_ for _ in ()).throw(AssertionError(name))
 c.run(b.entry if compiled else 0x82204f98);return c.mem.rd(S,64)
for i in range(128):
 mat=struct.pack('>16f',*[rng.uniform(-4,4) for _ in range(16)]);ang=struct.pack('>4f',*[rng.uniform(-2,2) for _ in range(4)]);a=run(False,mat,ang);z=run(True,mat,ang)
 if a!=z:print('case',i,'diff',[hex(k) for k,(x,y) in enumerate(zip(a,z)) if x!=y]);print(a.hex(),z.hex());raise AssertionError(i)
r={'cases':128,'scope':'Original ARTIST 82204F98 vs actual compiled RotateMatrix44AffineByEulerAnglesZXY; full 64-byte in-place matrix across finite randomized full inputs and four angle lanes, no mocked calls.','object':str(obj),'sha256':hashlib.sha256(obj.read_bytes()).hexdigest()};(O/'rotate-comparison-wip6.json').write_text(json.dumps(r,indent=2)+'\n');print(json.dumps(r,indent=2))

