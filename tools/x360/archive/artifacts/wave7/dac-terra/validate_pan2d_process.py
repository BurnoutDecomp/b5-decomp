import sys,json,random,struct,hashlib
from pathlib import Path
root=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(root/'scratch/x360_sdk'));import native_atomic_fixture as F
class PanCPU(F.AtomicCPU):
 def step(self,pc,word,nxt,depth):
  if word>>26==31 and ((word>>1)&1023)==278:return nxt
  return super().step(pc,word,nxt,depth)
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave7/dac-terra');p=out/'pan2d-build/Pan2D.obj';q=out/'rechannel-kernel-build/MixKernels.obj';b=F.Bundle([F.Coff(p,'?Process@Pan2D@'),F.Coff(q,'?CopyWithGain@')]);b.select('?Process@Pan2D@');errors=[];rng=random.Random(0x9ed8);selfaddr=F.M.S.SELF;aux=F.M.S.AUX;mix=0x50000000
for i in range(12):
 initial=bytearray(0x200);initial[0x21]=[2,4,6][i%3]
 for at,val in [(0x28,0.),(0x30,0.5),(0x38,1.),(0x40,1.),(0x48,0.),(0x50,0.),(0x54,0.5),(0x58,1.),(0x5c,1.),(0x60,0.)]:struct.pack_into('>f',initial,at,val)
 for j in range(6):struct.pack_into('>f',initial,0xd4+4*j,(j+1)/8.0)
 buf=bytearray(0x80);struct.pack_into('>I',buf,4,aux+0x8000);struct.pack_into('>H',buf,14,256);buf2=bytearray(buf);struct.pack_into('>I',buf2,4,aux+0xc000);both=[]
 for compiled in (False,True):
  c=F.cpu(b);c.__class__=PanCPU;c.mem.add(mix,0x31000);c.mem.wr(selfaddr,initial);c.mem.wr(aux+0x1000,buf);c.mem.wr(aux+0x2000,buf2);c.mem.wr(aux+0x8000,b''.join(struct.pack('>f',((j%31)-15)/8.0) for j in range(1536)));c.mem.wr(aux+0xc000,bytes.fromhex('3f000000')*1536);F.put(c,mix+0x3000c,aux+0x1000);F.put(c,mix+0x30010,aux+0x2000);c.setg(3,selfaddr);c.setg(4,mix);c.setg(5,0)
  for at,n in b.unresolved.items():
   if n.startswith(('__savefpr_', '__restfpr_')):c.hooks[at]=lambda x:None
  c.run(b.entry if compiled else 0x82b99ed8);both.append((c.mem.rd(selfaddr,0x200),c.mem.rd(mix+0x30000,0x40),c.mem.rd(aux+0xc000,6144),c.g(3)&0xffffffff))
 if both[0]!=both[1]:errors.append({'case':i,'objdiff':[hex(j) for j,(a,z) in enumerate(zip(both[0][0],both[1][0])) if a!=z][:20],'dstdiff':sum(a!=z for a,z in zip(both[0][2],both[1][2]))})
r={'cases':12,'errors':errors,'sha256':hashlib.sha256(p.read_bytes()).hexdigest(),'kernel_sha256':hashlib.sha256(q.read_bytes()).hexdigest(),'limits':'Actual Pan2D unchanged-attribute fast Process and CopyWithGain output/state, 2/4/6 channels; FPR save/restore controlled, no changed-attribute path or hardware claim.'};(out/'pan2d-process-comparison.json').write_text(json.dumps(r,indent=2)+'\n');print(len(errors),errors[:2]);assert not errors

