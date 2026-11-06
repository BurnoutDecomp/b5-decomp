import sys,json,random,struct,hashlib
from pathlib import Path
root=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(root/'scratch/x360_sdk'));import native_atomic_fixture as F
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave7/dac-terra');p=out/'rawpuller-native-build/RawPuller2.obj';b=F.Bundle([F.Coff(p,'?EventEvent@RawPuller2@')]);b.select('?EventEvent@RawPuller2@');errors=[];rng=random.Random(0xf358);selfaddr=F.M.S.SELF;aux=F.M.S.AUX
for i in range(128):
 img=bytearray(rng.randbytes(0x100));system=bytearray(rng.randbytes(0x1200));ring=bytearray(rng.randbytes(0x200));payload=struct.pack('>ffII',float(i%7),[22050.,48000.,96000.][i%3],aux+0x5000,aux+0x6000);struct.pack_into('>I',img,4,aux+0x1000);struct.pack_into('>I',system,0x20,aux+0x4000);cursor=(i%5)*8;struct.pack_into('>I',system,0x10b8,cursor);both=[]
 for compiled in (False,True):
  c=F.cpu(b);c.mem.wr(selfaddr,img);c.mem.wr(aux+0x1000,system);c.mem.wr(aux+0x4000,ring);c.mem.wr(aux+0x7000,payload);c.setg(3,selfaddr);c.setg(4,i&1);c.setg(5,aux+0x7000);c.run(b.entry if compiled else 0x82b9f358);handler=F.address(b,'?StopHandler@RawPuller2@' if i&1 else '?PlayHandler@RawPuller2@') if compiled else (0x82b9a5b0 if i&1 else 0x82b9a540);c.setg(3,aux+0x4000+cursor);c.run(handler);cmd=bytearray(c.mem.rd(aux+0x4000,0x200));cmd[cursor:cursor+4]=bytes(4);both.append((c.mem.rd(selfaddr,0x100),c.mem.rd(aux+0x1000,0x1200),cmd,c.g(3)&0xffffffff))
 if both[0]!=both[1]:errors.append({'case':i,'objdiff':[hex(j) for j,(a,z) in enumerate(zip(both[0][0],both[1][0])) if a!=z][:20]})
r={'cases':128,'errors':errors,'sha256':hashlib.sha256(p.read_bytes()).hexdigest(),'limits':'Actual EventEvent→actual Play/Stop handler, finite channel/rate values and typed pointers, command handler address normalized; no concurrency/hardware claim.'};(out/'rawpuller-event-handler-comparison.json').write_text(json.dumps(r,indent=2)+'\n');print(len(errors),errors[:2]);assert not errors
