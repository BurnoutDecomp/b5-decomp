import sys,json,random,struct,hashlib
from pathlib import Path
root=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(root/'scratch/x360_sdk'));import native_atomic_fixture as F
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave7/dac-terra');p=out/'resample-build/Resample.obj';b=F.Bundle([F.Coff(p,'?Process@Resample@')]);b.select('?Process@Resample@');errors=[];rng=random.Random(0xf3e8);selfaddr=F.M.S.SELF;aux=F.M.S.AUX;mix=0x50000000
for i in range(16):
 img=bytearray(rng.randbytes(0x200));img[0x21]=1+i%2;F32=lambda x:struct.pack('>f',x);struct.pack_into('>I',img,4,aux+0x3000);struct.pack_into('>f',img,0x38,48000);struct.pack_into('>I',img,0x3c,65536);struct.pack_into('>I',img,0x40,0);struct.pack_into('>H',img,0x44,0x50);struct.pack_into('>H',img,0x46,4);img[0x48]=2;img[0x49]=2;struct.pack_into('>12f',img,0x50,*[float(j)/8 for j in range(12)]);buf=bytearray(0x80);struct.pack_into('>I',buf,4,aux+0x8000);struct.pack_into('>H',buf,14,256);buf2=bytearray(buf);struct.pack_into('>I',buf2,4,aux+0xc000);both=[]
 for compiled in (False,True):
  c=F.cpu(b);c.mem.add(mix,0x31000);c.mem.wr(selfaddr,img);c.mem.wr(aux+0x1000,buf);c.mem.wr(aux+0x2000,buf2);c.mem.wr(aux+0x8000,b''.join(struct.pack('>f',((j%31)-15)/8.0) for j in range(512)));c.mem.wr(aux+0xc000,bytes.fromhex('3f000000')*512);F.put(c,aux+0x3000,aux+0x4000);F.put(c,aux+0x400c,aux+0xf000);F.put(c,aux+0x500c,struct.unpack('>I',F32(48000))[0]);F.put(c,mix+0x3000c,aux+0x1000);F.put(c,mix+0x30010,aux+0x2000);F.put(c,mix+0x30018,aux+0x5000);F.put(c,mix+0x30020,4);F.put(c,mix+0x30024,struct.unpack('>I',F32(48000 if i%4 else 44100))[0]);c.setg(3,selfaddr);c.setg(4,mix);c.setg(5,i&1)
  def memcpy(x):
   dst,src,n=[x.g(z)&0xffffffff for z in (3,4,5)];x.mem.wr(dst,x.mem.rd(src,n));x.setg(3,dst)
  c.hooks[0x82926ba0]=memcpy; c.hooks[0x82c0aab8]=memcpy
  for at,n in b.unresolved.items():
   if n=='memcpy':c.hooks[at]=memcpy
  c.run(b.entry if compiled else 0x82b9f3e8);both.append((c.mem.rd(selfaddr,0x200),c.mem.rd(mix+0x30000,0x40),c.mem.rd(aux+0xc000,2048),c.mem.rd(aux+0x4000,0x20),c.g(3)&0xffffffff))
 if both[0]!=both[1]:errors.append({'case':i,'objectdiff':[hex(j) for j,(a,z) in enumerate(zip(both[0][0],both[1][0])) if a!=z][:32], 'dstdiff':sum(a!=z for a,z in zip(both[0][2],both[1][2]))})
r={'cases':16,'errors':errors,'sha256':hashlib.sha256(p.read_bytes()).hexdigest(),'limits':'One/two channels; actual Process and interpolate, memcpy controlled; finite unity-ratio only; no hardware or full Voice claim.'};(out/'resample-process-comparison.json').write_text(json.dumps(r,indent=2)+'\n');print(len(errors),errors[:3]);assert not errors

