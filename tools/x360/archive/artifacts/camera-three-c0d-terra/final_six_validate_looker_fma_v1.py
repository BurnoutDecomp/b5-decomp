from pathlib import Path
import json,random,struct,sys,hashlib
P=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(P/'scratch/x360_sdk'));import native_atomic_fixture as F
O=Path('D:/Reverse/BP-X360-Experimental-Artifacts/camera-three-c0d-terra');j=json.loads((O/'final-six-v1/compile-results.json').read_text());obj=Path(next(x['object'] for x in j['results'] if x['source'].endswith('BrnLooker.cpp')));sym='?Zoom@Looker';b=F.Bundle([F.Coff(obj,sym)]);b.select(sym);A=F.M.S.AUX+0x5000

def run(compiled,x,y,z):
 c=F.cpu(b);c.mem.add(A,0x1000)
 if compiled:
  word=int.from_bytes(c.mem.rd(b.entry+0x5fc,4),'big');c.setf(13,x);c.setf(0,y);c.setf(1,z);out=30
 else:
  word=F.M.V.x360rd.u32(0x82222f48);c.setf(12,x);c.setf(13,y);c.setf(0,z);out=0
 c.mem.wr(A,struct.pack('>II',word,0x4e800020));c.run(A);return struct.pack('>f',c.f(out))
rng=random.Random(0x82222f48)
for i in range(256):
 vals=struct.unpack('>3f',struct.pack('>3f',*[rng.uniform(-10,10) for _ in range(3)]));a=run(False,*vals);z=run(True,*vals)
 if a!=z:raise AssertionError((i,vals,a.hex(),z.hex()))
r={'cases':256,'scope':'ARTIST 82222F48 scalar fmadds vs actual Looker::Zoom compiled fmadds; finite random binary32 inputs, exact result bits, isolated instruction slice.','object':str(obj),'sha256':hashlib.sha256(obj.read_bytes()).hexdigest()};(O/'looker-fma-comparison-final-six-v1.json').write_text(json.dumps(r,indent=2)+'\n');print(json.dumps(r,indent=2))
