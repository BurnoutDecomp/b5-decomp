import sys,struct
sys.path.insert(0,'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/scratch/x360_sdk');import native_atomic_fixture as F
from pathlib import Path
class WordCPU(F.AtomicCPU):
 def step(self,pc,word,nxt,depth):
  if word>>26==31 and ((word>>1)&1023)==28 and (word&1):
   src=(word>>21)&31;dest=(word>>16)&31;rb=(word>>11)&31;result=self.g(src)&self.g(rb);self.setg(dest,result);v=result&0xffffffff;self.setcr(0,bool(v&0x80000000),bool(v and not(v&0x80000000)),v==0);return nxt
  if word>>26==31 and ((word>>1)&1023)==792:
   src=(word>>21)&31;dest=(word>>16)&31;rb=(word>>11)&31;shift=self.g(rb)&0x3f;v=self.g(src)&0xffffffff;signed=(v&0x7fffffff)-(v&0x80000000);result=signed>>shift if shift<32 else (-1 if signed<0 else 0);self.setg(dest,result);self.xer_ca=int(signed<0 and (shift>=32 or (v&((1<<shift)-1))!=0));return nxt
  return super().step(pc,word,nxt,depth)
if __name__ == '__main__':
 b=F.Bundle([F.Coff(Path('D:/Reverse/BP-X360-Experimental-Artifacts/state-vtable-r3-terra/CgsState.obj'),'?IsAttachedToThis@State@')]);b.select('?IsAttachedToThis@State@')
 for addr in [0x82ba6e98,0x82ba74a8]:
  c=F.cpu(b);c.__class__=WordCPU;a=F.M.S.AUX;c.mem.wr(a+0x1000,b''.join(struct.pack('>f',0.1)for _ in range(128)));c.setg(3,F.M.S.SELF);c.setg(4,a+0x1000);c.setg(5,a+0x3000);c.setg(6,1);c.max_steps=1000000
  try:c.run(addr);print(hex(addr),'passed',c.mem.rd(a+0x3000,32).hex())
  except Exception as e:print(hex(addr),type(e).__name__,e,'last',hex(getattr(c,'last_pc',0)))

