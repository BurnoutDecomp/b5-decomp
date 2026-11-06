import sys,json,random,struct,hashlib
from pathlib import Path
root=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(root/'scratch/x360_sdk'));import native_atomic_fixture as F
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave7/dac-terra');p=out/'aiff-build/AiffWriter.obj';b=F.Bundle([F.Coff(p,'?EventEvent@AiffWriter@')]);b.select('?EventEvent@AiffWriter@');errors=[];rng=random.Random(0x5448);selfaddr=F.M.S.SELF;aux=F.M.S.AUX
for i in range(128):
 img=bytearray(rng.randbytes(0x100));system=bytearray(rng.randbytes(0x1200));ring=bytearray(rng.randbytes(0x200));struct.pack_into('>I',img,4,aux+0x1000);struct.pack_into('>I',system,0x20,aux+0x4000);cursor=(i%5)*4;struct.pack_into('>I',system,0x10b8,cursor);name=bytes([65+j%26 for j in range(i%40)])+b'\0';both=[]
 for compiled in (False,True):
  c=F.cpu(b);c.mem.wr(selfaddr,img);c.mem.wr(aux+0x1000,system);c.mem.wr(aux+0x4000,ring);c.mem.wr(aux+0x7100,name);F.put(c,aux+0x7000,aux+0x7100);c.setg(3,selfaddr);c.setg(4,i&1);c.setg(5,aux+0x7000);c.run(b.entry if compiled else 0x82ba5448);obj=c.mem.rd(selfaddr,0x100);command=bytearray(c.mem.rd(aux+0x4000,0x200));got=int.from_bytes(command[cursor:cursor+4],'big');
  if compiled:expected=F.address(b,'?StopHandler@AiffWriter@' if i&1 else '?StartHandler@AiffWriter@')
  else:expected=0x82ba1ec0 if i&1 else 0x82b9d510
  assert got==expected,(i,hex(got),hex(expected));command[cursor:cursor+4]=bytes(4);both.append((obj,c.mem.rd(aux+0x1000,0x1200),command))
 if both[0]!=both[1]:errors.append({'case':i,'systemdiff':[hex(j) for j,(a,z) in enumerate(zip(both[0][1],both[1][1])) if a!=z][:20],'ringdiff':[hex(j) for j,(a,z) in enumerate(zip(both[0][2],both[1][2])) if a!=z][:20]})
r={'cases':128,'errors':errors,'sha256':hashlib.sha256(p.read_bytes()).hexdigest(),'limits':'Actual EventEvent command state, path lengths 0–39, handler identities verified then normalized, no queue execution/hardware.'};(out/'aiff-event-comparison.json').write_text(json.dumps(r,indent=2)+'\n');print(len(errors),errors[:3]);assert not errors
