import sys,random,json,struct,hashlib
from pathlib import Path
sys.path.insert(0,r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\scratch\x360_sdk');import native_atomic_fixture as F
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/parallel/wave8_scene_terra/volume-baseline');obj=out/'triangle-volume-whole-wip.obj';b=F.Bundle([F.Coff(obj,'?LineSegIntersect@TriangleVolume@')]);b.select('?LineSegIntersect@TriangleVolume@');rng=random.Random(0x82bbb970);A=0x50000000;O=0x50001000;T=0x50002000;bad=[]
def target(c,pc):
 w=c.fetch(pc);d=w&0x3fffffc
 if d&0x2000000:d-=0x4000000
 return (pc+d)&0xffffffff
for i in range(128):
 src=bytearray(rng.randbytes(0x100));tm=bytearray(rng.randbytes(0x100));initial=rng.randbytes(0x100)
 for j in range(0,0x40,4):struct.pack_into('>f',src,j,rng.uniform(-10,10));struct.pack_into('>f',tm,j,rng.uniform(-3,3))
 vectors=[tuple(struct.unpack('>I',struct.pack('>f',rng.uniform(-10,10)))[0] for _ in range(4)) for _ in range(2)];f=rng.uniform(-2,2);r=[]
 for compiled in (False,True):
  c=F.cpu(b);c.mem.add(A,0x100,src);c.mem.add(O,0x100,initial);c.mem.add(T,0x100,tm);trace=[]
  def hook(cpu):trace.append((cpu.g(3)&0xffffffff,tuple(cpu.vr[k] for k in range(1,6)),cpu.f(1)));cpu.setg(3,1)
  if compiled:
   loc=next(a for a,n in b.unresolved.items() if 'TriangleLineSegIntersect' in n);c.hooks[loc]=hook
  else:c.hooks[target(c,0x82bbb9e0)]=hook
  c.setg(3,A);c.setg(4,T if i&1 else 0);c.setg(5,O);c.vr[1]=vectors[0];c.vr[2]=vectors[1];c.setf(1,f);c.run(b.entry if compiled else 0x82bbb970);r.append((c.mem.rd(O,0x100),c.mem.rd(A,0x100),c.mem.rd(T,0x100),trace,c.g(3)&0xffffffff))
 if r[0]!=r[1]:bad.append([i,r[0][3],r[1][3]])
print('cases 128 mismatches',len(bad),bad[:2]);assert not bad
(out/'triangle-line-wrapper-whole-wip-comparison.json').write_text(json.dumps({'cases':128,'mismatches':bad,'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'actual whole TriangleVolume CPP wrapper and GetPoints vs 82BBB970, null/non-null transform, capture v1-v5/f1/result pointer and guards, downstream TriangleLineSegIntersect controlled; no line-intersection provider claim'},indent=2))
