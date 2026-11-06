import sys,json,random,struct,hashlib
from pathlib import Path
root=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(root/'scratch/x360_sdk'));import native_atomic_fixture as F
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave7/dac-terra');p=out/'sine-native-build/SinePlayer.obj';errors=[];rng=random.Random(0x0068);selfaddr=F.M.S.SELF;aux=F.M.S.AUX;rows=[]
for name,orig in [('CreateInstance',0x82ba3fb8),('EventEvent',0x82ba0068)]:
 prefix='?'+name+'@SinePlayer@';b=F.Bundle([F.Coff(p,prefix)]);b.select(prefix)
 for i in range(128):
  img=bytearray(rng.randbytes(0x100));system=bytearray(rng.randbytes(0x1200));ring=bytearray(rng.randbytes(0x200));payload=rng.randbytes(8);struct.pack_into('>I',img,4,aux+0x1000);struct.pack_into('>I',system,0x20,aux+0x4000);cursor=(i%5)*8;struct.pack_into('>I',system,0x10b8,cursor);both=[]
  for compiled in (False,True):
   c=F.cpu(b);c.mem.wr(selfaddr,img);c.mem.wr(aux+0x1000,system);c.mem.wr(aux+0x4000,ring);c.mem.wr(aux+0x7000,payload);c.setg(3,selfaddr);c.setg(4,i&1);c.setg(5,aux+0x7000);c.run(b.entry if compiled else orig);obj=bytearray(c.mem.rd(selfaddr,0x100));cmd=bytearray(c.mem.rd(aux+0x4000,0x200));
   if name=='CreateInstance':obj[:4]=bytes(4)
   else:
    got=int.from_bytes(cmd[cursor:cursor+4],'big');expected=(F.address(b,'?StopHandler@SinePlayer@' if i&1 else '?PlayHandler@SinePlayer@') if compiled else (0x82b9b920 if i&1 else 0x82b9b900));assert got==expected;cmd[cursor:cursor+4]=bytes(4)
   both.append((obj,c.mem.rd(aux+0x1000,0x1200),cmd))
  if both[0]!=both[1]:errors.append({'name':name,'case':i,'objdiff':[hex(j) for j,(a,z) in enumerate(zip(both[0][0],both[1][0])) if a!=z][:15], 'cmddiff':[hex(j) for j,(a,z) in enumerate(zip(both[0][2],both[1][2])) if a!=z][:15]})
 rows.append({'name':name,'original':hex(orig)})
r={'cases':256,'errors':errors,'rows':rows,'sha256':hashlib.sha256(p.read_bytes()).hexdigest(),'limits':'Actual constructor and EventEvent, random raw 8-byte payload, handler IDs checked, vtable and handler address normalized; no queue execution/concurrency/hardware claim.'};(out/'sine-create-event-comparison.json').write_text(json.dumps(r,indent=2)+'\n');print(len(errors),errors[:1]);assert not errors
