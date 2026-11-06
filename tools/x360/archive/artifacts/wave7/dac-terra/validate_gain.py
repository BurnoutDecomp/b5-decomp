import sys,json,random,struct,hashlib
from pathlib import Path
root=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(root/'scratch/x360_sdk'));import native_atomic_fixture as F
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave7/dac-terra');r=json.loads((out/'gain-build/compile-results.json').read_text());p=Path(r['results'][0]['object']);selfaddr=F.M.S.SELF;aux=F.M.S.AUX;mix=0x50000000;errors=[];rng=random.Random(0x97600)
for kind,original,prefix in [('create',0x82ba2bd0,'?CreateInstance@Gain@'),('process',0x82b97600,'?Process@Gain@')]:
 b=F.Bundle([F.Coff(p,prefix)]);b.select(prefix)
 for i in range(128):
  img=bytearray(rng.randbytes(0x100));img[0x21]=i%5;struct.pack_into('>ff',img,0x28,float((i%17)-8)/8,0.0);struct.pack_into('>f',img,0x30,float((i%11)-5)/4);buf=bytearray(rng.randbytes(0x80));struct.pack_into('>I',buf,4,aux+0x8000);struct.pack_into('>H',buf,14,256);buf2=bytearray(buf);struct.pack_into('>I',buf2,4,aux+0xc000);both=[]
  for compiled in (False,True):
   c=F.cpu(b);c.mem.add(mix,0x31000);c.mem.wr(selfaddr,img);c.mem.wr(aux+0x1000,buf);c.mem.wr(aux+0x2000,buf2);F.put(c,mix+0x3000c,aux+0x1000);F.put(c,mix+0x30010,aux+0x2000);c.setg(3,selfaddr);c.setg(4,mix);c.setg(5,i&1);trace=[]
   def ramp(x):trace.append((x.g(3)&0xffffffff,x.g(4)&0xffffffff,x.g(7)&0xffffffff,struct.pack('>f',x.f(1)).hex(),struct.pack('>f',x.f(2)).hex()))
   c.hooks[0x82b67400]=ramp
   for at,n in b.unresolved.items():
    if n.startswith('?CopyWithGainRamp@'):c.hooks[at]=ramp
   c.run(b.entry if compiled else original);obj=bytearray(c.mem.rd(selfaddr,0x100));
   if kind=='create':obj[:4]=bytes(4)
   both.append((obj,c.mem.rd(mix+0x30000,0x40),trace,c.g(3)&0xffffffff))
  if both[0]!=both[1]:errors.append({'kind':kind,'case':i,'orig_trace':both[0][2],'compiled_trace':both[1][2]})
r={'cases':256,'mismatches':errors,'object':str(p),'sha256':hashlib.sha256(p.read_bytes()).hexdigest(),'original':['82BA2BD0','82B97600'],'limits':'Actual Gain constructor/process compared to original over random object bytes and finite gain inputs. CopyWithGainRamp controlled with full PPC argument trace, so kernel sample contents not validated; vtable identity normalized for constructor. No hardware or full link.'};(out/'gain-comparison.json').write_text(json.dumps(r,indent=2)+'\n');print('256 cases',len(errors),'mismatches');print(errors[:1]);assert not errors
