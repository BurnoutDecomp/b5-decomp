import sys,json,random,struct,hashlib
from pathlib import Path
root=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(root/'scratch/x360_sdk'));import native_atomic_fixture as F
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave7/dac-terra');p=out/'resample-build/Resample.obj';errors=[];rng=random.Random(0x37f0);selfaddr=F.M.S.SELF
for name,orig in [('CreateInstance',0x82ba37f0),('GetOutputSamples',0x82b9a8d0)]:
 prefix='?'+name+'@Resample@';b=F.Bundle([F.Coff(p,prefix)]);b.select(prefix)
 for i in range(128):
  initial=bytearray(rng.randbytes(0x200));initial[0x21]=i%7;initial[0x48]=i%6;initial[0x49]=2;struct.pack_into('>I',initial,0x3c,65536*(1+i%4));struct.pack_into('>I',initial,0x40,(i*1000)%65536);both=[]
  for compiled in (False,True):
   c=F.cpu(b);c.mem.wr(selfaddr,initial);c.setg(3,selfaddr);c.setg(4,i*2);trace=[]
   def memset(x):
    dst,val,n=[x.g(z)&0xffffffff for z in (3,4,5)];trace.append((dst,val,n));x.mem.wr(dst,bytes([val&255])*n);x.setg(3,dst)
   c.hooks[0x82926fd0]=memset
   for at,n in b.unresolved.items():
    if n=='memset':c.hooks[at]=memset
   c.run(b.entry if compiled else orig);obj=bytearray(c.mem.rd(selfaddr,0x200));
   if name=='CreateInstance':obj[:4]=bytes(4)
   both.append((obj,c.g(3)&0xffffffff,trace))
  if both[0]!=both[1]:errors.append({'name':name,'case':i,'diff':[hex(j) for j,(a,z) in enumerate(zip(both[0][0],both[1][0])) if a!=z][:32],'returns':[both[0][1],both[1][1]],'trace':[both[0][2],both[1][2]]})
r={'cases':256,'errors':errors,'sha256':hashlib.sha256(p.read_bytes()).hexdigest(),'limits':'Actual constructor and GetOutputSamples; constructor memset controlled and vtable normalized; aligned self, channels 0–6; no Process, hardware or full Voice claim.'};(out/'resample-create-output-comparison.json').write_text(json.dumps(r,indent=2)+'\n');print(len(errors),errors[:2]);assert not errors
