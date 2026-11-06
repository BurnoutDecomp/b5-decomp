import sys,random,json,struct,hashlib
from pathlib import Path
sys.path.insert(0,r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\scratch\x360_sdk')
import native_atomic_fixture as F
sys.path.insert(0,r"D:\Reverse\BP-Decomp_Workflow\tools\re");import x360rd
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/parallel/wave8_scene_terra/volume-baseline');obj=out/'fixable-wip2.obj';b=F.Bundle([F.Coff(obj,'?FixUp@FixableVolume@')]);rng=random.Random(0x828a88a0);A=0x50000000;D=0x50001000
class JumpCPU(F.AtomicCPU):
 def step(self,pc,word,nxt,depth):
  if pc in (0x828a87e4,0x828a8868): return self.ctr & 0xfffffffc
  return super().step(pc,word,nxt,depth)
def target(c,pc):
 w=c.fetch(pc); delta=w&0x3fffffc
 if delta&0x2000000:delta-=0x4000000
 return (pc+delta)&0xffffffff
bad=[]
for name,addr,calls in [('FixUp',0x828a87a0,[0x828a87fc,0x828a8814,0x828a8818]),('FixDown',0x828a8830,[0x828a8880,0x828a8898,0x828a889c])]:
 b.select('?'+name+'@FixableVolume@')
 for i in range(64):
  t=[0,6,7][i%3]; initial=bytearray(rng.randbytes(0x100));struct.pack_into('>I',initial,0x40,t if name=='FixUp' else D+0x20*t);result=[]
  for compiled in (False,True):
   c=F.cpu(b);c.__class__=JumpCPU;c.mem.add(A,0x100,initial);c.mem.add(D,0x200)
   for j in range(8):F.put(c,D+0x20*j,j)
   if not compiled:c.mem.add(0x820f2000,0x1000,x360rd.rd(0x820f2000,0x1000))
   if name=='FixUp':
    tab=next((x for x,n in b.unresolved.items() if 'gVolumeVTable' in n),None) if compiled else 0x8327eee0
    if not compiled:c.mem.add(tab,0x40)
    for j in range(8):F.put(c,tab+j*4,D+j*0x20)
   trace=[]
   def hook(kind):
    def f(cpu):
     if kind=='FireAssert':trace.append((kind,F.cstring(cpu,cpu.g(3)&0xffffffff),F.cstring(cpu,cpu.g(4)&0xffffffff),cpu.g(5)))
     else:trace.append((kind,))
    return f
   if compiled:
    for a,n in b.unresolved.items():
     for kind in ['BeginAssert','FireAssert','EndAssert']:
      if kind in n:c.hooks[a]=hook(kind)
   else:
    for pc,kind in zip(calls,['BeginAssert','FireAssert','EndAssert']):c.hooks[target(c,pc)]=hook(kind)
   c.setg(3,A);c.run(b.entry if compiled else addr);result.append((c.mem.rd(A,0x100).hex(),trace))
  if result[0]!=result[1]:bad.append([name,i,result])
print('cases 128, mismatches',len(bad),bad[:1]);assert not bad
(out/'fixable-invalid-wip-comparison.json').write_text(json.dumps({'cases':128,'mismatches':bad,'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'invalid 0/6/7, poisoned object and guards, controlled descriptor/table and assertion calls including message, path, line; WIP'},indent=2))
