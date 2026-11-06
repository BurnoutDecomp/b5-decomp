from pathlib import Path
import sys,json,random,struct,hashlib
sys.path.insert(0,r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\scratch\x360_sdk');import native_atomic_fixture as F
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/parallel/wave8_scene_terra/triangle-edge');obj=out/'probe.obj';name='?TriangleEdges@@';b=F.Bundle([F.Coff(obj,name)]);b.select(name);A=0x40000000;rng=random.Random(0x82276868);bad=[]
def orig(x,y):
 c=F.cpu(b);c.vr[1]=x;c.vr[2]=y;c.run(0x82276868);return struct.pack('>4I',*c.vr[1])
for i in range(128):
 v=[struct.unpack('>4I',struct.pack('>4f',*[rng.uniform(-1000,1000) for j in range(4)])) for k in range(3)];c=F.cpu(b);c.mem.add(A,0x1000);c.mem.wr(A,b''.join(struct.pack('>4I',*x) for x in v));c.mem.wr(A+0x100,b'\xA5'*64);c.setg(3,A);c.setg(4,A+0x110);c.setg(5,A+0x130);c.run(b.entry);expected=[orig(v[1],v[0]),orig(v[2],v[1])];actual=[c.mem.rd(A+0x110,16),c.mem.rd(A+0x130,16)]
 if actual!=expected or c.mem.rd(A+0x100,16)!=b'\xA5'*16 or c.mem.rd(A+0x120,16)!=b'\xA5'*16:bad.append(i)
r={'cases':128,'mismatches':bad,'scope':'actual compiled Triangle edge inline consumer vs original VPU Subtract82276868, finite xyz/W random vectors and guard bytes; no entire triangle intersection or NaN claim','object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest()};(out/'comparison.json').write_text(json.dumps(r,indent=2)+'\n');print('cases',128,'mismatches',len(bad));assert not bad
