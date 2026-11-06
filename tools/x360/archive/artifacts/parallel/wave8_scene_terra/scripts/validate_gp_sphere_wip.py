import sys,random,json,struct,hashlib
from pathlib import Path
sys.path.insert(0,r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\scratch\x360_sdk');import native_atomic_fixture as F
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/parallel/wave8_scene_terra/volume-baseline');obj=out/'GPSphere-typed-wip.obj';b=F.Bundle([F.Coff(obj,'?GetMaximumFeature@GPSphere@')]);rng=random.Random(0x82ba8088);A=0x50000000;O=0x50001000;D=0x50002000;bad=[]
for name,addr in [('?GetMaximumFeature@GPSphere@',0x82ba8088),('?GetInterval@GPSphere@',0x82ba80a8),('?GetIntervals@GPSphere@',0x82ba80c0)]:
 b.select(name)
 for i in range(128):
  src=bytearray(rng.randbytes(0x100));dirs=bytearray(rng.randbytes(0x100));initial=rng.randbytes(0x300)
  for j in range(0,0x10,4):struct.pack_into('>f',src,j,rng.uniform(-10,10))
  for j in range(0,0x50,4):struct.pack_into('>f',dirs,j,rng.uniform(-10,10))
  v=tuple(rng.getrandbits(32) for _ in range(4));r=[]
  for compiled in (False,True):
   c=F.cpu(b);c.mem.add(A,0x100,src);c.mem.add(O,0x300,initial);c.mem.add(D,0x100,dirs);c.setg(3,A);c.vr[1]=v
   if 'MaximumFeature' in name:c.setg(4,i&1);c.setg(5,O)
   elif 'GetIntervals' in name:c.setg(4,D);c.setg(5,i%5);c.setg(6,O)
   else:c.setg(4,O)
   c.run(b.entry if compiled else addr);r.append((c.mem.rd(O,0x300),c.mem.rd(A,0x100),c.mem.rd(D,0x100)))
  if r[0]!=r[1]:bad.append([name,i,r[0][0][:64].hex(),r[1][0][:64].hex()])
print('cases 384 mismatches',len(bad),bad[:1]);assert not bad
(out/'gp-sphere-wip-comparison.json').write_text(json.dumps({'cases':384,'mismatches':bad,'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'actual whole GPSphere provider vs 82BA8088/80A8/80C0: v1 value ABI, zero/multiple direction loops, full feature/interval/input guards; finite input rows, no dispatch/table claim'},indent=2))
