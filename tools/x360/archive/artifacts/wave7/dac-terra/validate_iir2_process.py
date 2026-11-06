import sys,json,random,struct,hashlib
from pathlib import Path
root=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(root/'scratch/x360_sdk'));import native_atomic_fixture as F
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave7/dac-terra');d=out/'iir2-build';p=d/'LowPassIir2.obj';q=d/'Iir2-foreign-3534.obj';b=F.Bundle([F.Coff(p,'?Process@LowPassIir2@'),F.Coff(q,'?Filter@Iir2@')]);b.select('?Process@LowPassIir2@');errors=[];rng=random.Random(0xe5c8);selfaddr=F.M.S.SELF;aux=F.M.S.AUX;mix=0x50000000
for i in range(8):
 initial=bytearray(0x200);initial[0x21]=1+i%2;struct.pack_into('>f',initial,0x28,0.0);struct.pack_into('>f',initial,0xa4,0.003141593);struct.pack_into('>5f',initial,0x90,0.0,0.0,1.0,0.0,0.0);buf=bytearray(0x80);struct.pack_into('>I',buf,4,aux+0x8000);struct.pack_into('>H',buf,14,256);buf2=bytearray(buf);struct.pack_into('>I',buf2,4,aux+0xc000);both=[]
 for compiled in (False,True):
  c=F.cpu(b);c.mem.add(mix,0x31000);c.mem.wr(selfaddr,initial);c.mem.wr(aux+0x1000,buf);c.mem.wr(aux+0x2000,buf2);c.mem.wr(aux+0x8000,b''.join(struct.pack('>f',((j%31)-15)/8.0) for j in range(512)));c.mem.wr(aux+0xc000,bytes.fromhex('3f000000')*512);F.put(c,aux+0x500c,struct.unpack('>I',struct.pack('>f',48000.))[0]);F.put(c,mix+0x3000c,aux+0x1000);F.put(c,mix+0x30010,aux+0x2000);F.put(c,mix+0x30018,aux+0x5000);c.setg(3,selfaddr);c.setg(4,mix);
  for at,n in b.unresolved.items():
   if n.startswith(('__savefpr_', '__restfpr_')):c.hooks[at]=lambda x:None
  c.run(b.entry if compiled else 0x82b9e5c8);both.append((c.mem.rd(selfaddr,0x200),c.mem.rd(mix+0x30000,0x40),c.mem.rd(aux+0xc000,2048),c.g(3)&0xffffffff))
 if both[0]!=both[1]:errors.append({'case':i,'objectdiff':[hex(j) for j,(a,z) in enumerate(zip(both[0][0],both[1][0])) if a!=z][:24],'dstdiff':sum(a!=z for a,z in zip(both[0][2],both[1][2]))})
r={'cases':8,'errors':errors,'sha256':hashlib.sha256(p.read_bytes()).hexdigest(),'foreign_sha256':hashlib.sha256(q.read_bytes()).hexdigest(),'limits':'Actual LowPass Process and actual Iir2::Filter, unchanged cached low-frequency coefficients with unity feedforward; one/two channels. No other family process/general coefficients/hardware claim.'};(out/'iir2-lowpass-process-comparison.json').write_text(json.dumps(r,indent=2)+'\n');print(len(errors),errors[:2]);assert not errors

