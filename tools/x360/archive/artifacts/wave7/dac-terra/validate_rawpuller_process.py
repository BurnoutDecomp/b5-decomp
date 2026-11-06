import sys,json,random,struct,hashlib
from pathlib import Path
root=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(root/'scratch/x360_sdk'));import native_atomic_fixture as F
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave7/dac-terra');p=out/'rawpuller-native-build/RawPuller2.obj';b=F.Bundle([F.Coff(p,'?Process@RawPuller2@')]);b.select('?Process@RawPuller2@');errors=[];rng=random.Random(0xa5d8);selfaddr=F.M.S.SELF;aux=F.M.S.AUX;mix=0x50000000;callback=aux+0x7000
for i in range(128):
 img=bytearray(rng.randbytes(0x100));struct.pack_into('>I',img,0x24,callback if i%8 else 0);struct.pack_into('>I',img,0x28,aux+0x5000);struct.pack_into('>f',img,0x2c,48000.);struct.pack_into('>I',img,0x30,4);img[0x34]=(i//4)&1;img[0x35]=1+i%3;buf=bytearray(0x80);struct.pack_into('>I',buf,4,aux+0x8000);struct.pack_into('>H',buf,14,256);both=[]
 for compiled in (False,True):
  c=F.cpu(b);c.mem.add(mix,0x31000);c.mem.add(0x8327a000,0x5000);c.mem.wr(selfaddr,img);c.mem.wr(aux+0x1000,buf);c.mem.wr(aux+0x8000,bytes.fromhex('3f000000')*768);F.put(c,mix+0x3000c,aux+0x2000);F.put(c,mix+0x30010,aux+0x1000);F.put(c,mix+0x30028,struct.unpack('>I',struct.pack('>f',0.5))[0]);c.setg(3,selfaddr);c.setg(4,mix);trace=[]
  def pull(x):
   n=x.g(3)&0xffffffff;dst=x.g(4)&0xffffffff;trace.append(('pull',n,x.g(6)&0xffffffff,struct.pack('>f',x.f(1)).hex()));x.mem.wr(dst,b''.join(struct.pack('>f',float(j+1)/8) for j in range(n*img[0x35])));x.setg(3,[0,1,256,257][i%4])
  def copy(x):
   dst,src,n=[x.g(z)&0xffffffff for z in (3,4,5)];trace.append(('copy',dst,n));x.mem.wr(dst,x.mem.rd(src,n));x.setg(3,dst)
  c.hooks[callback]=pull;c.hooks[0x82926ba0]=copy
  for at,n in b.unresolved.items():
   if n in ('memcpy','XMemCpy'):c.hooks[at]=copy
  c.run(b.entry if compiled else 0x82b9a5d8);both.append((c.mem.rd(selfaddr,0x100),c.mem.rd(mix+0x30000,0x40),c.mem.rd(aux+0x8000,3072),trace,c.g(3)&0xffffffff))
 if both[0]!=both[1]:errors.append({'case':i,'trace':[both[0][3],both[1][3]],'returns':[both[0][4],both[1][4]],'samplediff':sum(a!=z for a,z in zip(both[0][2],both[1][2]))})
r={'cases':128,'errors':errors,'sha256':hashlib.sha256(p.read_bytes()).hexdigest(),'limits':'Actual RawPuller2 Process with callback and memcpy controlled; scratch address normalized via fixture; tests callback low-byte returns 0,1,0x100,0x101, null/reprime and finite 1–3 channels. Native scratch global remains unresolved; no scheduler/hardware claim.'};(out/'rawpuller-process-comparison.json').write_text(json.dumps(r,indent=2)+'\n');print(len(errors),errors[:1]);assert not errors
