import sys,json,random,hashlib
from pathlib import Path
root=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(root/'scratch/x360_sdk'));import native_atomic_fixture as F
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave7/dac-terra');p=out/'limiter-build/Limiter1.obj';errors=[];rng=random.Random(0x2f28);selfaddr=F.M.S.SELF
b=F.Bundle([F.Coff(p,'?CreateInstance@Limiter1@')]);b.select('?CreateInstance@Limiter1@')
for i in range(128):
 initial=bytearray(rng.randbytes(0x200));both=[]
 for compiled in (False,True):
  c=F.cpu(b);c.mem.wr(selfaddr,initial);c.setg(3,selfaddr);trace=[]
  def memset(x):
   dst,val,n=[x.g(z)&0xffffffff for z in (3,4,5)];trace.append((dst,val,n));x.mem.wr(dst,bytes([val&255])*n);x.setg(3,dst)
  def clear(x):
   dst=x.g(3)&0xffffffff;trace.append((dst,0,48));x.mem.wr(dst,bytes(48));x.setg(3,dst)
  c.hooks[0x82926fd0]=memset
  for at,n in b.unresolved.items():
   if n.startswith('?ClearBuffer@CompressorLimiter1@'):c.hooks[at]=clear
  c.run(b.entry if compiled else 0x82ba2f28);obj=bytearray(c.mem.rd(selfaddr,0x200));obj[:4]=bytes(4);both.append((obj,c.g(3)&0xffffffff,trace))
 if both[0]!=both[1]:errors.append({'case':i,'diff':[hex(j) for j,(a,z) in enumerate(zip(both[0][0],both[1][0])) if a!=z], 'trace':[both[0][2],both[1][2]]})
r={'cases':128,'errors':errors,'sha256':hashlib.sha256(p.read_bytes()).hexdigest(),'limits':'Actual constructor; original memset and compiled ClearBuffer controlled; vtable normalized; compressor Process provider unavailable.'};(out/'limiter-create-comparison.json').write_text(json.dumps(r,indent=2)+'\n');print(len(errors),errors[:2]);assert not errors
