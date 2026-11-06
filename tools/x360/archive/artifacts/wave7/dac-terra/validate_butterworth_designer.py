import sys,json,random,struct,hashlib,math
from pathlib import Path
root=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(root/'scratch/x360_sdk'));import native_atomic_fixture as F
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave7/dac-terra');p=out/'butterworth-build/Butterworth.obj';b=F.Bundle([F.Coff(p,'?CalculateFilterCoefficients@Butterworth@')]);b.select('?CalculateFilterCoefficients@Butterworth@');errors=[];rng=random.Random(0x4698);selfaddr=F.M.S.SELF
for i in range(64):
 img=bytearray(rng.randbytes(0x100));both=[];order=1+i%4;kind=(i//4)%2;cutoff=float(500+(i%10)*500);shape=[0.5,1.,2.][(i//8)%3]
 for compiled in (False,True):
  c=F.cpu(b);c.mem.wr(selfaddr,img);c.setg(3,selfaddr);c.setg(5,order);c.setg(7,kind);c.setf(1,cutoff);c.setf(2,48000.);c.setf(3,shape)
  def memset(x):
   dst,val,n=[x.g(z)&0xffffffff for z in (3,4,5)];x.mem.wr(dst,bytes([val&255])*n);x.setg(3,dst)
  def tan(x):x.setf(1,math.tan(x.f(1)))
  def powf(x):x.setf(1,math.pow(x.f(1),x.f(2)))
  c.hooks[0x82926fd0]=memset;c.hooks[0x82c09e70]=tan;c.hooks[0x82c09970]=powf
  for at,n in b.unresolved.items():
   if n=='memset':c.hooks[at]=memset
   if n=='tan':c.hooks[at]=tan
   if n=='pow':c.hooks[at]=powf
   if n.startswith(('__savefpr_', '__restfpr_')):c.hooks[at]=lambda x:None
  c.run(b.entry if compiled else 0x82b64698);both.append(c.mem.rd(selfaddr,0x100))
 if both[0]!=both[1]:errors.append({'case':i,'order':order,'type':kind,'shape':shape,'diff':[hex(j) for j,(a,z) in enumerate(zip(both[0],both[1])) if a!=z][:32], 'orig':both[0][:40].hex(),'compiled':both[1][:40].hex()})
r={'cases':64,'errors':errors,'sha256':hashlib.sha256(p.read_bytes()).hexdigest(),'limits':'Actual designer original/compiled, tan/pow/memset/FPR helpers controlled, finite inputs; no filter/hardware claim.'};(out/'butterworth-designer-comparison.json').write_text(json.dumps(r,indent=2)+'\n');print(len(errors),errors[:2]);assert not errors
