import sys,json,random,struct,hashlib
from pathlib import Path
root=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(root/'scratch/x360_sdk'));import native_atomic_fixture as F
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave7/dac-terra');p=out/'pause-build/Pause.obj';b=F.Bundle([F.Coff(p,'?Process@Pause@')]);b.select('?Process@Pause@');errors=[];rng=random.Random(0xa218);selfaddr=F.M.S.SELF;aux=F.M.S.AUX;mix=0x50000000
for i in range(96):
 img=bytearray(rng.randbytes(0x100));img[0x21]=1+i%2;struct.pack_into('>f',img,0x28,[0.0,1.0,0.5][i%3]);struct.pack_into('>f',img,0x30,0.5);struct.pack_into('>H',img,0x34,8);img[0x36]=[8,16][i%2];img[0x37]=[0,1,2,3][(i//3)%4];buf=bytearray(0x80);struct.pack_into('>I',buf,4,aux+0x8000);struct.pack_into('>H',buf,14,256);buf2=bytearray(buf);struct.pack_into('>I',buf2,4,aux+0xc000);both=[]
 for compiled in (False,True):
  c=F.cpu(b);c.mem.add(mix,0x31000);c.mem.wr(selfaddr,img);c.mem.wr(aux+0x1000,buf);c.mem.wr(aux+0x2000,buf2);c.mem.wr(aux+0x8000,b''.join(struct.pack('>f',((j%31)-15)/8.0) for j in range(512)));c.mem.wr(aux+0xc000,bytes.fromhex('3f000000')*512);F.put(c,mix+0x3000c,aux+0x1000);F.put(c,mix+0x30010,aux+0x2000);F.put(c,mix+0x30020,8);c.setg(3,selfaddr);c.setg(4,mix);c.setg(5,i&1)
  def memset(x):
   dst,val,n=[x.g(z)&0xffffffff for z in (3,4,5)];x.mem.wr(dst,bytes([val&255])*n);x.setg(3,dst)
  c.hooks[0x82926fd0]=memset
  for at,n in b.unresolved.items():
   if 'memset' in n.lower():c.hooks[at]=memset
  c.run(b.entry if compiled else 0x82b9a218);both.append((c.mem.rd(selfaddr,0x100),c.mem.rd(mix+0x30000,0x40),c.mem.rd(aux+0x8000,2048),c.mem.rd(aux+0xc000,2048),c.g(3)&0xffffffff))
 if both[0]!=both[1]:errors.append({'case':i,'objectdiff':[hex(j) for j,(a,z) in enumerate(zip(both[0][0],both[1][0])) if a!=z], 'srcdiff':sum(a!=z for a,z in zip(both[0][2],both[1][2])),'dstdiff':sum(a!=z for a,z in zip(both[0][3],both[1][3]))})
r={'cases':96,'errors':errors,'sha256':hashlib.sha256(p.read_bytes()).hexdigest(),'limits':'Finite controlled input, 1–2 channels, original/compiled actual Process, memset controlled; no hardware/full Voice claim.'};(out/'pause-process-comparison.json').write_text(json.dumps(r,indent=2)+'\n');print(len(errors),errors[:3]);assert not errors
