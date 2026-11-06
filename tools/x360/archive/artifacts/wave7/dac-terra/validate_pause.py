import sys,json,random,struct,hashlib
from pathlib import Path
root=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(root/'scratch/x360_sdk'));import native_atomic_fixture as F
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave7/dac-terra');p=out/'pause-build/Pause.obj';errors=[];rng=random.Random(0xa140);selfaddr=F.M.S.SELF
for name,orig in [('CreateInstance',0x82ba36b8),('PreProcess',0x82b9a140)]:
 prefix='?'+name+'@Pause@';b=F.Bundle([F.Coff(p,prefix)]);b.select(prefix)
 for i in range(128):
  initial=bytearray(rng.randbytes(0x100));struct.pack_into('>f',initial,0x28,[0.0,1.0,0.5,-0.5][i%4]);initial[0x36]=i%65;initial[0x37]=(i//3)%6;initial[0x38]=i%2;both=[]
  for compiled in (False,True):
   c=F.cpu(b);c.mem.wr(selfaddr,initial);c.setg(3,selfaddr);c.setg(4,0);c.setg(5,(i//2)%2);c.setg(6,(i*13)%512);c.run(b.entry if compiled else orig);obj=bytearray(c.mem.rd(selfaddr,0x100));
   if name=='CreateInstance':obj[:4]=bytes(4)
   both.append((obj,c.g(3)&0xffffffff))
  if both[0]!=both[1]:errors.append({'name':name,'case':i,'diff':[hex(j) for j,(a,b) in enumerate(zip(both[0][0],both[1][0])) if a!=b], 'returns':[both[0][1],both[1][1]]})
r={'cases':256,'errors':errors,'sha256':hashlib.sha256(p.read_bytes()).hexdigest(),'limits':'Actual constructor and PreProcess; finite input cases; constructor vtable normalized; no Process, dispatch, hardware or full Voice claim.'};(out/'pause-create-preprocess-comparison.json').write_text(json.dumps(r,indent=2)+'\n');print(len(errors),errors[:2]);assert not errors
