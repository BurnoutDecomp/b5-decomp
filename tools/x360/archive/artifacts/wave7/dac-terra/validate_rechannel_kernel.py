import sys,json,random,struct,hashlib
from pathlib import Path
root=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(root/'scratch/x360_sdk'));import native_atomic_fixture as F
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave7/dac-terra');p=out/'rechannel-kernel-build/Rechannel.obj';kernelobj=out/'rechannel-kernel-build/MixKernels.obj';selfaddr=F.M.S.SELF;aux=F.M.S.AUX;mix=0x50000000;errors=[];rng=random.Random(0xa728)
for kind,original,prefix in [('process',0x82b9a728,'?Process@Rechannel@')]:
 b=F.Bundle([F.Coff(p,prefix),F.Coff(kernelobj,'?ReChannelGainWrite@')]);b.select(prefix)
 for i in range(16):
  img=bytearray(rng.randbytes(0x100));img[0x21]=1+i%2;buf=bytearray(rng.randbytes(0x80));struct.pack_into('>I',buf,4,aux+0x8000);struct.pack_into('>H',buf,14,256);buf2=bytearray(buf);struct.pack_into('>I',buf2,4,aux+0xc000);both=[]
  for compiled in (False,True):
   c=F.cpu(b);c.mem.add(mix,0x31000);c.mem.wr(selfaddr,img);c.mem.wr(aux+0x1000,buf);c.mem.wr(aux+0x2000,buf2);c.mem.wr(aux+0x8000, b''.join(struct.pack('>f', ((j%31)-15)/8.0) for j in range(1536)));c.mem.wr(aux+0xc000, bytes.fromhex('3f000000')*1536);F.put(c,mix+0x3000c,aux+0x1000);F.put(c,mix+0x30010,aux+0x2000);F.put(c,mix+0x30020,8);c.mem.wr(mix+0x3002c,bytes([1+(i//2)%2]));c.setg(3,selfaddr);c.setg(4,mix);trace=[]
   c.run(b.entry if compiled else original);obj=bytearray(c.mem.rd(selfaddr,0x100));
   if kind=='create':obj[:4]=bytes(4)
   both.append((obj,c.mem.rd(mix+0x30000,0x40),c.mem.rd(aux+0xc000,6144),c.g(3)&0xffffffff))
  if both[0]!=both[1]:errors.append({'kind':kind,'case':i,'original_output':both[0][2][:64].hex(),'compiled_output':both[1][2][:64].hex()})
r={'cases':16,'mismatches':errors,'original':'82B9A728 -> 82B6B268','note':'Actual compiled Rechannel and kernel; compare sample output/state'};(out/'rechannel-kernel-comparison.json').write_text(json.dumps(r,indent=2)+'\n');print('16 cases',len(errors),'mismatches');print(errors[:1]);assert not errors


