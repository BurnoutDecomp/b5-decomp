from pathlib import Path
import json,random,struct,sys,hashlib
P=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(P/'scratch/x360_sdk'));import native_atomic_fixture as F
O=Path('D:/Reverse/BP-X360-Experimental-Artifacts/camera-three-c0d-terra');j=json.loads((O/'rotate-vehicle-v2/compile-results.json').read_text());obj=Path(j['results'][0]['object']);b=F.Bundle([F.Coff(obj,'?ScaleVector@')]);b.select('?ScaleVector@');scale=b.entry;ba=F.Bundle([F.Coff(obj,'?ScaleAndAdd@')]);ba.select('?ScaleAndAdd@');add=ba.entry;A=F.M.S.AUX;CODE=A+0x5000

def run(compiled,cols,scales):
 c=F.cpu(b)
 if compiled:
  for i,col in enumerate(cols):c.mem.wr(A+16*i,col)
  c.setf(1,scales[0]);c.setg(4,A+32);c.run(scale);acc=struct.pack('>4I',*c.vr[1]);c.mem.wr(A+0x100,acc)
  for i,colidx in enumerate([1,0,0,1,2],1):
   prev=c.mem.rd(A+0x100,16);c=F.cpu(ba);c.mem.wr(A+0x100,prev);c.mem.wr(A+16*colidx,cols[colidx]);c.setg(3,A+0x100);c.setg(5,A+16*colidx);c.setf(1,scales[i]);c.run(add)
  return c.mem.rd(A+0x100,16)
 c.mem.add(CODE,0x1000);pcs=[0x8224a3e8,0x8224a3ec,0x8224a3f0,0x8224a3f8,0x8224a400,0x8224a404];c.mem.wr(CODE,b''.join(struct.pack('>I',F.M.V.x360rd.u32(pc)) for pc in pcs)+struct.pack('>I',0x4e800020));
 for r,i in [(12,0),(9,1),(10,2)]:c.vr[r]=struct.unpack('>4I',cols[i])
 for r,i in [(8,0),(4,1),(11,2),(7,3),(6,4),(5,5)]:c.vr[r]=struct.unpack('>4I',struct.pack('>4f',*[scales[i]]*4))
 c.run(CODE);return struct.pack('>4I',*c.vr[0])
rng=random.Random(0x8224a3e8)
for i in range(256):
 cols=[struct.pack('>4f',*[rng.uniform(-30,30) for _ in range(4)]) for _ in range(3)];scales=struct.unpack('>6f',struct.pack('>6f',*[rng.uniform(-30,30) for _ in range(6)]));a=run(False,cols,scales);z=run(True,cols,scales)
 if a!=z:raise AssertionError((i,a.hex(),z.hex()))
r={'cases':256,'scope':'ARTIST 8224A3E8..A404 six-op VMX multiply and fused accumulation vs actual compiled ScaleVector/ScaleAndAdd composed, all four result lanes, finite random vectors and scalars.','object':str(obj),'sha256':hashlib.sha256(obj.read_bytes()).hexdigest()};(O/'rotate-vehicle-accum-comparison-v1.json').write_text(json.dumps(r,indent=2)+'\n');print(json.dumps(r,indent=2))
