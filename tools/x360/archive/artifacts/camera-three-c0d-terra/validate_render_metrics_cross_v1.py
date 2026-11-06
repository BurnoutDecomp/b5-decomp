from pathlib import Path
import json,random,struct,sys,hashlib
P=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(P/'scratch/x360_sdk'));import native_atomic_fixture as F
O=Path('D:/Reverse/BP-X360-Experimental-Artifacts/camera-three-c0d-terra');j=json.loads((O/'render-metrics-v4/compile-results.json').read_text());obj=Path(j['results'][0]['object']);sym='?CrossConsole@';b=F.Bundle([F.Coff(obj,sym)]);b.select(sym);A=F.M.S.AUX;CODE=A+0x5000

def run(compiled,lhs,rhs):
 c=F.cpu(b)
 if compiled:c.mem.wr(A,lhs);c.mem.wr(A+16,rhs);c.setg(3,A);c.setg(4,A+16);c.run(b.entry);return tuple(c.vr[1])
 c.mem.add(CODE,0x1000);c.mem.wr(CODE,b''.join(struct.pack('>I',F.M.V.x360rd.u32(pc)) for pc in [0x8224929c,0x822492b0,0x822492b4,0x822492b8,0x822492bc])+struct.pack('>I',0x4e800020));c.vr[0]=struct.unpack('>4I',lhs);c.vr[13]=struct.unpack('>4I',rhs);c.vr[11]=struct.unpack('>4I',rhs);c.run(CODE);return tuple(c.vr[0])
rng=random.Random(0x8224929c)
for i in range(256):
 lhs=struct.pack('>4f',*[rng.uniform(-100,100) for _ in range(4)]);rhs=struct.pack('>4f',*[rng.uniform(-100,100) for _ in range(4)]);a=run(False,lhs,rhs);z=run(True,lhs,rhs)
 if a!=z:print(i,a,z);raise AssertionError(i)
r={'cases':256,'scope':'ARTIST 8224929C/B0..BC cross-product instruction slice vs actual compiled CrossConsole; complete four raw output lanes from finite random vectors.','object':str(obj),'sha256':hashlib.sha256(obj.read_bytes()).hexdigest()};(O/'render-metrics-cross-comparison-v1.json').write_text(json.dumps(r,indent=2)+'\n');print(json.dumps(r,indent=2))
