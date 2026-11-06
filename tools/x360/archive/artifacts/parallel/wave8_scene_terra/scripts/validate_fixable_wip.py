import sys,random,json,struct,hashlib
from pathlib import Path
sys.path.insert(0,r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\scratch\x360_sdk')
import native_atomic_fixture as F
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/parallel/wave8_scene_terra/volume-baseline')
obj=out/'fixable-wip2.obj';b=F.Bundle([F.Coff(obj,'?FixUp@FixableVolume@')]);rng=random.Random(0x828a87a0);A=0x50000000;D=0x50001000
bad=[]
class JumpCPU(F.AtomicCPU):
 def step(self,pc,word,nxt,depth):
  if pc in (0x828a87e4,0x828a8868): return self.ctr & 0xfffffffc
  return super().step(pc,word,nxt,depth)
for name,addr in [('FixUp',0x828a87a0),('FixDown',0x828a8830)]:
 b.select('?'+name+'@FixableVolume@')
 for i in range(128):
  t=i%5+1; initial=bytearray(rng.randbytes(0x100));struct.pack_into('>I',initial,0x40,t if name=='FixUp' else D+0x20*t)
  result=[]
  for compiled in (False,True):
   c=F.cpu(b);c.__class__=JumpCPU;c.mem.add(A,0x100,initial);c.mem.add(D,0x100)
   for j in range(1,6):F.put(c,D+0x20*j,j)
   if name=='FixUp':
    tab=next((x for x,n in b.unresolved.items() if 'gVolumeVTable' in n),None) if compiled else 0x8327eee0
    
    if not compiled: c.mem.add(tab,0x40)
    for j in range(1,6):F.put(c,tab+j*4,D+j*0x20)
   c.setg(3,A);c.run(b.entry if compiled else addr);result.append(c.mem.rd(A,0x100))
  if result[0]!=result[1]:bad.append([name,i])
print('cases 256, mismatches',len(bad),bad[:4]);assert not bad
(out/'fixable-wip-comparison.json').write_text(json.dumps({'cases':256,'mismatches':bad,'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'valid type 1..5, full 0x100 poisoned object and guard bytes, controlled descriptor table; WIP and no assertion-path coverage'},indent=2))
