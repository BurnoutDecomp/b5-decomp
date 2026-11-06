import sys,random,json,hashlib,struct
from pathlib import Path
sys.path.insert(0,r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\scratch\x360_sdk')
import native_atomic_fixture as F
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/parallel/wave8_scene_terra/volume-baseline');obj=out/'AggregateVolume-wip-v4.obj';b=F.Bundle([F.Coff(obj,'?GetBBox@AggregateVolume@')]);rng=random.Random(0x82bbba10);A=0x50000000;D=0x50001000;T=0x50002000;O=0x50003000;bad=[]
def target(c,pc):
 w=c.fetch(pc);d=w&0x3fffffc
 if d&0x2000000:d-=0x4000000
 return (pc+d)&0xffffffff
for prefix,entry,callpc in [('?GetBBox@AggregateVolume@',0x82bbba10,0x82bbbb18),('?GetBBoxDiag@AggregateVolume@',0x82bbbb58,0x82bbbba0)]:
 b.select(prefix)
 for i in range(128):
  def floats(n):return b''.join(struct.pack('>f',rng.uniform(-25,25)) for _ in range(n))
  src=bytearray(rng.randbytes(0x100));src[:64]=floats(16);struct.pack_into('>I',src,0x44,D);agg=floats(8)+rng.randbytes(0xe0);tm=floats(16)+rng.randbytes(0xc0);dest=rng.randbytes(0x100);res=floats(8);got=[]
  for compiled in (False,True):
   c=F.cpu(b);c.mem.add(A,0x100,src);c.mem.add(D,0x100,agg);c.mem.add(T,0x100,tm);c.mem.add(O,0x100,dest);trace=[]
   def hook(cpu):
    a=cpu.g(3)&0xffffffff;bb=cpu.g(4)&0xffffffff;m=cpu.g(5)&0xffffffff
    trace.append([cpu.mem.rd(bb,32).hex(),cpu.mem.rd(m,64).hex()]);cpu.mem.wr(a,res);cpu.setg(3,a)
   if compiled:
    at=next(a for a,n in b.unresolved.items() if 'Transform@AABBox' in n);c.hooks[at]=hook
   else:c.hooks[target(c,callpc)]=hook
   if 'Diag' in prefix:c.setg(3,O);c.setg(4,A)
   else:c.setg(3,A);c.setg(4,T if i&1 else 0);c.setg(5,i&1);c.setg(6,O)
   c.run(b.entry if compiled else entry);got.append((c.mem.rd(O,0x100).hex(),c.mem.rd(A,0x100).hex(),c.mem.rd(D,0x100).hex(),c.mem.rd(T,0x100).hex(),trace))
  if got[0]!=got[1]:bad.append([prefix,i,got])
print('cases 256, mismatches',len(bad),bad[:1]);assert not bad
(out/'aggregate-wip-v4-comparison.json').write_text(json.dumps({'cases':256,'mismatches':bad,'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'GetBBox null/non-null transform and GetBBoxDiag finite lanes, full controlled AAB callee input/output and object guards; AAB callee controlled, no hardware estimate claim'},indent=2))
