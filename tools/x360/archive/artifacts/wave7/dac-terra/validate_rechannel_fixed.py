import sys,json,random,struct,hashlib
from pathlib import Path
root=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(root/'scratch/x360_sdk'));import native_atomic_fixture as F
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave7/dac-terra');p=out/'rechannel-kernel-build/Rechannel.obj';selfaddr=F.M.S.SELF;aux=F.M.S.AUX;mix=0x50000000;errors=[];rng=random.Random(0xa728)
for kind,original,prefix in [('create',0x82ba37d0,'?CreateInstance@Rechannel@'),('process',0x82b9a728,'?Process@Rechannel@')]:
 b=F.Bundle([F.Coff(p,prefix)]);b.select(prefix)
 for i in range(128):
  img=bytearray(rng.randbytes(0x100));img[0x21]=i%9;buf=bytearray(rng.randbytes(0x80));struct.pack_into('>I',buf,4,aux+0x8000);struct.pack_into('>H',buf,14,256);buf2=bytearray(buf);struct.pack_into('>I',buf2,4,aux+0xc000);both=[]
  for compiled in (False,True):
   c=F.cpu(b);c.mem.add(mix,0x31000);c.mem.wr(selfaddr,img);c.mem.wr(aux+0x1000,buf);c.mem.wr(aux+0x2000,buf2);F.put(c,mix+0x3000c,aux+0x1000);F.put(c,mix+0x30010,aux+0x2000);F.put(c,mix+0x30020,0 if i&1 else 256);c.mem.wr(mix+0x3002c,bytes([(i//2)%9]));c.setg(3,selfaddr);c.setg(4,mix);trace=[]
   def kernel(x):
    dst,src,nd,ns,n=[x.g(z)&0xffffffff for z in (3,4,6,7,8)];trace.append((nd,ns,n,struct.pack('>f',x.f(1)).hex(),x.mem.rd(dst,(i%9)*4).hex(),x.mem.rd(src,min((i//2)%9,6)*4).hex()))
   c.hooks[0x82b6b268]=kernel
   for at,n in b.unresolved.items():
    if n.startswith('?ReChannelGainWrite@'):c.hooks[at]=kernel
   c.run(b.entry if compiled else original);obj=bytearray(c.mem.rd(selfaddr,0x100));
   if kind=='create':obj[:4]=bytes(4)
   both.append((obj,c.mem.rd(mix+0x30000,0x40),trace,c.g(3)&0xffffffff))
  if both[0]!=both[1]:errors.append({'kind':kind,'case':i,'original_trace':both[0][2],'compiled_trace':both[1][2]})
r={'cases':256,'mismatches':errors,'object':str(p),'sha256':hashlib.sha256(p.read_bytes()).hexdigest(),'original':['82BA37D0','82B9A728'],'limits':'Actual compiled Rechannel constructor/process against original; channel remap kernel controlled with pointer arrays and argument trace, not sample contents. Constructor vtable addresses normalized. No hardware/full link.'};(out/'rechannel-fixed-comparison.json').write_text(json.dumps(r,indent=2)+'\n');print('256 cases',len(errors),'mismatches');print(errors[:1]);assert not errors
