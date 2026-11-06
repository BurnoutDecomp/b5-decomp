import sys,json,random,struct,hashlib
from pathlib import Path
root=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(root/'scratch/x360_sdk'));import native_atomic_fixture as F
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave7/dac-terra');d=out/'rechannel-kernel-build';p=d/'Send.obj';k=d/'MixKernels.obj';b=F.Bundle([F.Coff(p,'?DisconnectImmediate@Send@'),F.Coff(k,'?ReChannelGainWrite@')]);b.select('?DisconnectImmediate@Send@');errors=[];rng=random.Random(0xfe38);selfaddr=F.M.S.SELF
for i in range(32):
 img=bytearray(rng.randbytes(0x100));img[0x20]=1+i%2;img[0x40]=1+(i//2)%2
 for j in range(6):struct.pack_into('>f',img,0x44+4*j,((i+j)%19-9)/8)
 both=[]
 for compiled in (False,True):
  c=F.cpu(b);c.mem.wr(selfaddr,img);c.setg(3,selfaddr);trace=[]
  def disconnect(x):
   ptr=x.g(4)&0xffffffff;trace.append((x.g(3)&0xffffffff,x.mem.rd(ptr,img[0x40]*4).hex() if ptr else None));x.setg(3,0x12345678)
  c.hooks[0x82b9c3c0]=disconnect
  for at,name in b.unresolved.items():
   if name.startswith('?Disconnect@SubMixConnector@'):c.hooks[at]=disconnect
  c.run(b.entry if compiled else 0x82b9fe38);both.append((c.mem.rd(selfaddr,0x100),trace,c.g(3)&0xffffffff))
 if both[0]!=both[1]:errors.append({'case':i,'original':str(both[0][1:]),'compiled':str(both[1][1:])})
r={'cases':32,'mismatches':errors,'original':'82B9FE38 -> 82B6B268','object_sha256':{x.name:hashlib.sha256(x.read_bytes()).hexdigest() for x in [p,k]},'limits':'Actual Send and kernel, real gain data and foldback, controlled SubMixConnector::Disconnect. No hardware/concurrency claim.'};(out/'send-kernel-comparison.json').write_text(json.dumps(r,indent=2)+'\n');print('32 cases',len(errors),'mismatches',errors[:1]);assert not errors
