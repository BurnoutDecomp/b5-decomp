from pathlib import Path
import json,sys,random,hashlib
P=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(P/'scratch/x360_sdk'));import native_atomic_fixture as F
O=Path('D:/Reverse/BP-X360-Experimental-Artifacts/director-526-terra');j=json.loads((O/'world-status-probe/compile-results.json').read_text());obj=Path(j['results'][0]['object']);setter=F.Coff(obj,'SetStatusProbe');getter=F.Coff(obj,'GetAllStreamedProbe');SELF=F.M.S.SELF;SRC=F.M.S.AUX+0x1000
class StopCPU(F.AtomicCPU):
 def fetch(self,pc):
  if pc==self.stop:raise F.CPU.run.__globals__['Return']()
  return super().fetch(pc)
rng=random.Random(0x823e3f88)
for i in range(128):
 initial=rng.randbytes(32); src=bytes(rng.randrange(2) for _ in range(5));outs=[]
 for orig in [True,False]:
  if orig:
   c=F.M.new_cpu();c.__class__=StopCPU;c.stop=0x823e3fa8;c.setg(26,SELF);c.setg(11,SRC);start=0x823e3f88
  else:
   c=setter.cpu();c.setg(3,SELF);c.setg(4,SRC);start=setter.entry
  c.mem.wr(SELF+0x6aa8,initial);c.mem.wr(SRC,src);c.run(start);outs.append(c.mem.rd(SELF+0x6aa8,32))
 assert outs[0]==outs[1],i
 for orig in [True,False]:
  if orig:
   c=F.M.new_cpu();c.__class__=StopCPU;c.stop=0x82250c68;c.setg(10,SELF);start=0x82250c64
  else:
   c=getter.cpu();c.setg(3,SELF);start=getter.entry
  c.mem.wr(SELF+0x6aa8,outs[0]);c.run(start);assert (c.g(5) if orig else c.g(3))==src[4],(orig,i)
r={'cases':128,'object':str(obj),'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'Compare native compiled actual-header status setter against ARTIST 823E3F88..A4 five-byte copy across poisoned surrounding bytes; compare native getter against original 82250C64 all-streamed byte load. No full bridge/gameplay claim.'};(O/'world-status-comparison.json').write_text(json.dumps(r,indent=2)+'\n');print(json.dumps(r,indent=2))
