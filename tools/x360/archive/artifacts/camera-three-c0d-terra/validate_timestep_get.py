from pathlib import Path
import json,random,struct,sys,hashlib
P=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(P/'scratch/x360_sdk'));import native_atomic_fixture as F
O=Path('D:/Reverse/BP-X360-Experimental-Artifacts/camera-three-c0d-terra');j=json.loads((O/'shadow-wip1/compile-results.json').read_text());obj=Path(next(x['object'] for x in j['results'] if x['source'].endswith('BrnDirectorTimestep.cpp')));sym='?GetVecFloat@Timestep@BrnDirector@@';b=F.Bundle([F.Coff(obj,sym)]);b.select(sym);S=F.M.S.SELF;OUT=F.M.S.AUX+0x1000;rng=random.Random(0x821f2978)
def run(compiled,raw,index):
 c=F.cpu(b);c.mem.wr(S,raw);c.mem.wr(OUT,b'\xa5'*16);c.setg(3,OUT);c.setg(4,S);c.setg(5,index)
 for at,n in b.unresolved.items():c.hooks[at]=lambda cc,name=n:(_ for _ in ()).throw(AssertionError(name))
 c.run(b.entry if compiled else 0x821f2978);return c.g(3),c.mem.rd(OUT,16)
for i in range(128):
 raw=rng.randbytes(64);index=i%3;assert run(False,raw,index)==run(True,raw,index),i
r={'cases':128,'scope':'Original 821F2978 vs actual compiled Timestep::GetVecFloat; genuine SDK member return sret r3, this r4, index r5, complete16 raw bits and returned pointer; valid indices only.','object':str(obj),'sha256':hashlib.sha256(obj.read_bytes()).hexdigest()};(O/'timestep-get-comparison-wip.json').write_text(json.dumps(r,indent=2)+'\n');print(json.dumps(r,indent=2))
