import sys,json,random,struct,hashlib,math,ctypes
from pathlib import Path
root=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(root/'scratch/x360_sdk'));import native_atomic_fixture as F
fma=ctypes.CDLL('ucrtbase').fma;fma.argtypes=[ctypes.c_double]*3;fma.restype=ctypes.c_double
class SqrtCPU(F.AtomicCPU):
 def step(self,pc,word,nxt,depth):
  if word>>26==63 and ((word>>1)&1023)==22:
   self.setf((word>>21)&31,math.sqrt(self.f((word>>11)&31)));return nxt
  if word>>26==63 and ((word>>1)&31)==28:
   self.setf((word>>21)&31,fma(self.f((word>>16)&31),self.f((word>>6)&31),-self.f((word>>11)&31)));return nxt
  return super().step(pc,word,nxt,depth)
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave7/dac-terra');p=out/'pan2d1-build/Pan2D1.obj';b=F.Bundle([F.Coff(p,'?ComputeLevels@Pan2D1@')]);b.select('?ComputeLevels@Pan2D1@');errors=[];rng=random.Random(0x91d8);selfaddr=F.M.S.SELF
for i in range(64):
 img=bytearray(0x300);struct.pack_into('>I',img,0xcc,1);struct.pack_into('>I',img,0xd0,[2,4,5][i%3]);struct.pack_into('>f',img,0x48,0.5);struct.pack_into('>f',img,0x50,1.0);struct.pack_into('>f',img,0x58,0.2);struct.pack_into('>f',img,0x84,1.0)
 for j,angle in enumerate([0.6,0.,-0.6,2.3,-2.3]):struct.pack_into('>2f',img,0x13c+j*8,math.cos(angle),math.sin(angle))
 for j in range(16):struct.pack_into('>f',img,0x8c+4*j,[1.,0.,0.,1.][j%4])
 x=((i%15)-7)/10.;y=(((i*7)%15)-7)/10.;struct.pack_into('>4f',img,0x164,x,y,(x*x+y*y)*0.5,math.atan2(y,x));both=[]
 for compiled in (False,True):
  c=F.cpu(b);c.__class__=SqrtCPU;c.mem.wr(selfaddr,img);c.setg(3,selfaddr)
  c.hooks[0x82c0cae0]=lambda x:x.setf(1,float(math.floor(x.f(1))));c.hooks[0x82c095d0]=lambda x:x.setf(1,math.sin(x.f(1)));c.hooks[0x82c096a0]=lambda x:x.setf(1,math.cos(x.f(1)))
  for at,n in b.unresolved.items():
   if n=='floor':c.hooks[at]=lambda x:x.setf(1,float(math.floor(x.f(1))))
   if n=='sin':c.hooks[at]=lambda x:x.setf(1,math.sin(x.f(1)))
   if n=='cos':c.hooks[at]=lambda x:x.setf(1,math.cos(x.f(1)))
   if n.startswith(('__savefpr_', '__restfpr_')):c.hooks[at]=lambda x:None
  c.run(b.entry if compiled else 0x82b991d8);both.append(c.mem.rd(selfaddr,0x300))
 if both[0]!=both[1]:errors.append({'case':i,'diff':[hex(j) for j,(a,z) in enumerate(zip(both[0],both[1])) if a!=z][:32]})
r={'cases':64,'errors':errors,'sha256':hashlib.sha256(p.read_bytes()).hexdigest(),'limits':'Actual ComputeLevels with actual interior/perimeter; controlled sin/cos/floor and FPR save/restore, private CRT fma model, finite 2/4/5 speaker cases; no hardware claim.'};(out/'pan2d1-levels-comparison.json').write_text(json.dumps(r,indent=2)+'\n');print(len(errors),errors[:3]);assert not errors




