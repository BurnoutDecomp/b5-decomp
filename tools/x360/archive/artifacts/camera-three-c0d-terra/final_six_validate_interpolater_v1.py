from pathlib import Path
import json,random,struct,sys,hashlib
P=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(P/'scratch/x360_sdk'));import native_atomic_fixture as F
O=Path('D:/Reverse/BP-X360-Experimental-Artifacts/camera-three-c0d-terra');j=json.loads((O/'final-six-v1/compile-results.json').read_text());obj=Path(next(x['object'] for x in j['results'] if x['source'].endswith('BrnInterpolater.cpp')));sym='?Construct@Interpolater';b=F.Bundle([F.Coff(obj,sym)]);b.select(sym);S=F.M.S.SELF;CODE=F.M.S.AUX+0x5000

def run(compiled,state):
 c=F.cpu(b);c.mem.wr(S,state)
 if compiled:c.setg(3,S);c.run(b.entry)
 else:
  c.mem.add(CODE,0x1000);c.mem.wr(CODE,b''.join(struct.pack('>I',F.M.V.x360rd.u32(pc)) for pc in [0x82255fe0,0x82255fe4,0x82256014,0x82256018])+struct.pack('>I',0x4e800020));c.setg(11,S);c.run(CODE)
 return c.mem.rd(S,len(state))
rng=random.Random(0x82255fc8)
for i in range(128):
 state=rng.randbytes(32);a=run(False,state);z=run(True,state)
 if a!=z:raise AssertionError((i,a.hex(),z.hex()))
r={'cases':128,'scope':'Original 82255FE0/FE4/56014/56018 inlined Interpolater initialization vs actual compiled Construct, poisoned full32-byte state including all four zero vector lanes and unchanged trailing bytes.','object':str(obj),'sha256':hashlib.sha256(obj.read_bytes()).hexdigest()};(O/'interpolater-construct-comparison-final-six-v1.json').write_text(json.dumps(r,indent=2)+'\n');print(json.dumps(r,indent=2))
