import sys,json,random,hashlib
from pathlib import Path
root=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(root/'scratch/x360_sdk'));import native_atomic_fixture as F
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave7/dac-terra');d=out/'compressor-combined-build';q=d/'CompressorLimiter1.obj';errors=[];rng=random.Random(0x2f28);selfaddr=F.M.S.SELF;rows=[]
for name,orig in [('Compressor1',0x82ba2408),('Limiter1',0x82ba2f28)]:
 p=d/(name+'.obj');pre='?CreateInstance@'+name+'@';b=F.Bundle([F.Coff(p,pre),F.Coff(q,'?ClearBuffer@CompressorLimiter1@')]);b.select(pre)
 for i in range(128):
  initial=bytearray(rng.randbytes(0x200));both=[]
  for compiled in (False,True):
   c=F.cpu(b);c.mem.wr(selfaddr,initial);c.setg(3,selfaddr);trace=[]
   def memset(x):
    dst,val,n=[x.g(z)&0xffffffff for z in (3,4,5)];trace.append((dst,val,n));x.mem.wr(dst,bytes([val&255])*n);x.setg(3,dst)
   c.hooks[0x82926fd0]=memset
   for at,n in b.unresolved.items():
    if n=='memset':c.hooks[at]=memset
   c.run(b.entry if compiled else orig);obj=bytearray(c.mem.rd(selfaddr,0x200));obj[:4]=bytes(4);both.append((obj,c.g(3)&0xffffffff,trace))
  if both[0]!=both[1]:errors.append({'name':name,'case':i,'diff':[hex(j) for j,(a,z) in enumerate(zip(both[0][0],both[1][0])) if a!=z][:20],'trace':[both[0][2],both[1][2]]})
 rows.append({'name':name,'sha256':hashlib.sha256(p.read_bytes()).hexdigest()})
r={'cases':256,'errors':errors,'rows':rows,'provider_sha256':hashlib.sha256(q.read_bytes()).hexdigest(),'limits':'Actual Compressor1/Limiter1 constructors→actual CompressorLimiter1::ClearBuffer; memset controlled, vtable normalized. No heap/hardware claim.'};(out/'compressor-combined-create-comparison.json').write_text(json.dumps(r,indent=2)+'\n');print(len(errors),errors[:1]);assert not errors
