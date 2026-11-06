import sys,json,random,struct,hashlib
from pathlib import Path
root=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(root/'scratch/x360_sdk'));import native_atomic_fixture as F
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave7/dac-terra');d=out/'snd9-native-build';p=d/'Snd9Service.obj';b=F.Bundle([F.Coff(p,'?Process@Snd9Service@')]);b.select('?Process@Snd9Service@');errors=[];rng=random.Random(0xbd80);selfaddr=F.M.S.SELF;aux=F.M.S.AUX;mix=0x50000000
for i in range(32):
 img=bytearray(rng.randbytes(0x200));buf=bytearray(0x80);struct.pack_into('>I',buf,4,aux+0xc000);struct.pack_into('>H',buf,14,256)
 for j in range(6):struct.pack_into('>I',img,0x40+j*4,aux+0x8000+j*0x400)
 src=rng.randbytes(6144);dest=rng.randbytes(6144);both=[]
 for compiled in (False,True):
  c=F.cpu(b);c.mem.add(mix,0x31000);c.mem.wr(selfaddr,img);c.mem.wr(aux+0x1000,buf);c.mem.wr(aux+0x8000,src);c.mem.wr(aux+0xc000,dest);F.put(c,mix+0x3000c,aux+0x2000);F.put(c,mix+0x30010,aux+0x1000);c.setg(3,selfaddr);c.setg(4,mix);trace=[]
  def copy(x):
   dst,src,n=[x.g(z)&0xffffffff for z in (3,4,5)];trace.append((dst,src,n));x.mem.wr(dst,x.mem.rd(src,n));x.setg(3,dst)
  c.hooks[0x82926ba0]=copy
  for at,n in b.unresolved.items():
   if n in ('memcpy','XMemCpy'):c.hooks[at]=copy
  c.run(b.entry if compiled else 0x82b9bd80);both.append((c.mem.rd(selfaddr,0x200),c.mem.rd(mix+0x30000,0x40),c.mem.rd(aux+0xc000,6144),trace,c.g(3)&0xffffffff))
 if both[0]!=both[1]:errors.append({'case':i,'trace':[both[0][3],both[1][3]],'samplediff':sum(a!=z for a,z in zip(both[0][2],both[1][2]))})
r={'cases':32,'errors':errors,'sha256':hashlib.sha256(p.read_bytes()).hexdigest(),'limits':'Actual Snd9 Process six-channel data and context compared, memcpy controlled as byte copies; no real mixer/driver/hardware claim.'};(out/'snd9-process-comparison.json').write_text(json.dumps(r,indent=2)+'\n');print(len(errors),errors[:1]);assert not errors
