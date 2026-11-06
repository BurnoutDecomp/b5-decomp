import sys,json,random,struct,hashlib,math
from pathlib import Path
root=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(root/'scratch/x360_sdk'));import native_atomic_fixture as F
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave7/dac-terra');p=out/'compressor-native-build/Compressor1.obj';b=F.Bundle([F.Coff(p,'?Configure@Compressor1@')]);b.select('?Configure@Compressor1@');errors=[];rng=random.Random(0x6b28);selfaddr=F.M.S.SELF
for i in range(128):
 img=bytearray(rng.randbytes(0x200))
 for at,val in [(0x28,[-600.,-30.,0.,10.][i%4]),(0x30,[1.,2.,4.][i%3]),(0x38,[-1.,0.1,12.][(i//3)%3]),(0x40,[-1.,0.5,31.][(i//5)%3]),(0x48,float(i%2))]:struct.pack_into('>f',img,at,val)
 both=[]
 for compiled in (False,True):
  c=F.cpu(b);c.mem.wr(selfaddr,img);c.setg(3,selfaddr);c.setf(1,48000.);trace=[]
  def powf(x):x.setf(1,math.pow(x.f(1),x.f(2)))
  def configure(x):trace.append((x.g(3)&0xffffffff,*[x.g(z)&0xffffffff for z in (7,8,9)],struct.pack('>3f',x.f(1),x.f(2),x.f(3)).hex()))
  c.hooks[0x82c09970]=powf;c.hooks[0x82b67188]=configure
  for at,n in b.unresolved.items():
   if n=='pow':c.hooks[at]=powf
   if n.startswith('?Configure@CompressorLimiter1@'):c.hooks[at]=configure
   if n.startswith(('__savefpr_', '__restfpr_')):c.hooks[at]=lambda x:None
  c.run(b.entry if compiled else 0x82b96b28);both.append((c.mem.rd(selfaddr,0x200),trace))
 if both[0]!=both[1]:errors.append({'case':i,'diff':[hex(j) for j,(a,z) in enumerate(zip(both[0][0],both[1][0])) if a!=z][:20], 'trace':[both[0][1],both[1][1]]})
r={'cases':128,'errors':errors,'sha256':hashlib.sha256(p.read_bytes()).hexdigest(),'limits':'Actual Compressor1 Configure original vs compiled, pow and CompressorLimiter1 Configure and FPR save/restore controlled, parameter traces compared, finite inputs; no compressor kernel/hardware claim.'};(out/'compressor-configure-comparison.json').write_text(json.dumps(r,indent=2)+'\n');print(len(errors),errors[:2]);assert not errors
