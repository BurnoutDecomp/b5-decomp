import sys,random,json,struct,hashlib
from pathlib import Path
sys.path.insert(0,r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\scratch\x360_sdk');import native_atomic_fixture as F
sys.path.insert(0,r'D:\Reverse\BP-Decomp_Workflow\tools\re');import x360rd
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/parallel/wave8_scene_terra/volume-baseline');obj=out/'cylinder-line-dispatch-probe.obj';b=F.Bundle([F.Coff(obj,'?LineSegIntersect@CylinderVolume@')]);b.select('?LineSegIntersect@CylinderVolume@');rng=random.Random(0x82baf688);A=0x50000000;bad=[]
class TailCPU(F.AtomicCPU):
 def step(self,pc,word,nxt,depth):
  if self.start<=pc<self.start+0x40 and word>>26==18 and not(word&1):
   d=word&0x3fffffc
   if d&0x2000000:d-=0x4000000
   target=(pc+d)&0xffffffff
   name=self.targets.get(target)
   if name:
    self.traceResult=(name,tuple(self.g(i)&0xffffffff for i in range(3,8)),self.f(1));self.setg(3,7);raise F.CPU.run.__globals__['Return']()
  return super().step(pc,word,nxt,depth)
for i in range(128):
 src=bytearray(rng.randbytes(0x100));rad=rng.uniform(-10,10);fat=-rad if i%2==0 else rng.uniform(-10,10);struct.pack_into('>f',src,0x50,rad);rad=struct.unpack_from('>f',src,0x50)[0]
 if i%2==0:fat=-rad
 r=[]
 for compiled in (False,True):
  c=F.cpu(b);c.__class__=TailCPU;c.mem.add(A,0x100,src)
  if not compiled:c.mem.add(0x82001000,0x1000,x360rd.rd(0x82001000,0x1000))
  c.start=b.entry if compiled else 0x82baf688;c.targets={}
  if compiled:
   for addr,name in b.unresolved.items():
    if 'ThinLineSegIntersect' in name:c.targets[addr]='thin'
    if 'FatLineSegIntersect' in name:c.targets[addr]='fat'
  else:c.targets={0x82badce0:'thin',0x82baeb10:'fat'}
  for k in range(3,8):c.setg(k,A+(k-3)*0x10)
  c.setf(1,fat);c.run(c.start);r.append((c.traceResult,c.mem.rd(A,0x100)))
 if r[0]!=r[1]:bad.append([i,r[0][0],r[1][0]])
print('cases 128 mismatches',len(bad),bad[:2]);assert not bad
(out/'cylinder-dispatch-probe-comparison.json').write_text(json.dumps({'cases':128,'mismatches':bad,'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'temporary Cylinder dispatcher; finite zero/nonzero sums, captures tail target/r3-r7/f1, original 82BAF688, downstream methods controlled; NOT whole provider'},indent=2))
