from pathlib import Path
import json,random,struct,sys,hashlib
P=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(P/'scratch/x360_sdk'));import native_atomic_fixture as F
O=Path('D:/Reverse/BP-X360-Experimental-Artifacts/camera-three-c0d-terra');j=json.loads((O/'aftertouch-v1/compile-results.json').read_text());obj=Path(j['results'][0]['object']);sym='?Scale@';b=F.Bundle([F.Coff(obj,sym)]);b.select(sym);A=F.M.S.AUX;CODE=A+0x5000

def run(compiled,vec,scale):
 c=F.cpu(b)
 if compiled:c.mem.wr(A,vec);c.setg(3,A);c.setf(1,scale);c.run(b.entry);return tuple(c.vr[1])
 c.mem.add(CODE,0x1000);c.mem.wr(CODE,struct.pack('>II',F.M.V.x360rd.u32(0x821fa42c),0x4e800020));c.vr[12]=struct.unpack('>4I',vec);c.vr[13]=struct.unpack('>4I',struct.pack('>4f',*[scale]*4));c.run(CODE);return tuple(c.vr[27])
rng=random.Random(0x821fa42c)
for i in range(256):
 vec=struct.pack('>4f',*[rng.uniform(-20,20) for _ in range(4)]);scale=struct.unpack('>f',struct.pack('>f',rng.uniform(-20,20)))[0];a=run(False,vec,scale);z=run(True,vec,scale)
 if a!=z:raise AssertionError((i,a,z))
r={'cases':256,'scope':'ARTIST 821FA42C vmulfp128 vs actual compiled Aftertouch Scale, all four raw lanes, finite random inputs.','object':str(obj),'sha256':hashlib.sha256(obj.read_bytes()).hexdigest()};(O/'aftertouch-scale-comparison-v1.json').write_text(json.dumps(r,indent=2)+'\n');print(json.dumps(r,indent=2))
