import sys,random,json,hashlib
from pathlib import Path
sys.path.insert(0,r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\scratch\x360_sdk')
import native_atomic_fixture as F
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/parallel/wave8_scene_terra/volume-baseline');obj=out/'volume-wip4.obj';b=F.Bundle([F.Coff(obj,'??4Volume@collision@rw@@')]);b.select('??4Volume@collision@rw@@');r=random.Random(0x82bb12d0);A=0x50000000;B=0x50001000;bad=[]
for i in range(128):
 a=r.randbytes(0x100);src=r.randbytes(0x100);results=[]
 for compiled in (False,True):
  c=F.cpu(b);c.mem.add(A,0x100,a);c.mem.add(B,0x100,src)
  for at,n in b.unresolved.items():
   if n=='memcpy':
    def copy(c):c.mem.wr(c.g(3)&0xffffffff,c.mem.rd(c.g(4)&0xffffffff,c.g(5)&0xffffffff))
    c.hooks[at]=copy
  c.setg(3,A);c.setg(4,B);c.run(b.entry if compiled else 0x82bb12d0);results.append((c.mem.rd(A,0x100),c.mem.rd(B,0x100),c.g(3)&0xffffffff))
 if results[0]!=results[1]:bad.append(i)
print('cases',128,'mismatches',len(bad),bad[:4]);assert not bad
(out/'volume-assignment-wip-comparison.json').write_text(json.dumps({'cases':128,'mismatches':bad,'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'nonoverlap poisoned source/destination/guards, memcpy controlled; original vs compiled assignment, WIP only'},indent=2))
