import sys,json,random,struct,hashlib
from pathlib import Path
root=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(root/'scratch/x360_sdk'));import native_atomic_fixture as F
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave7/dac-terra');d=out/'sine-native-build';p=d/'SinePlayer.obj';q=d/'sine_probe.obj';b=F.Bundle([F.Coff(q,'?NativeSineEventDispatch'),F.Coff(p,'?CreateInstance@SinePlayer@')]);b.select('?NativeSineEventDispatch');errors=[];rng=random.Random(0x0068);selfaddr=F.M.S.SELF;aux=F.M.S.AUX;assert F.M.V.x360rd.u32(0x8217f544+4)==0x82ba0068
for i in range(128):
 img=bytearray(rng.randbytes(0x100));system=bytearray(rng.randbytes(0x1200));ring=bytearray(rng.randbytes(0x200));payload=rng.randbytes(8);struct.pack_into('>I',img,4,aux+0x1000);struct.pack_into('>I',system,0x20,aux+0x4000);cursor=(i%5)*8;struct.pack_into('>I',system,0x10b8,cursor);both=[]
 for compiled in (False,True):
  c=F.cpu(b);c.mem.wr(selfaddr,img);c.mem.wr(aux+0x1000,system);c.mem.wr(aux+0x4000,ring);c.mem.wr(aux+0x7000,payload)
  if not compiled:F.put(c,selfaddr,0x8217f544)
  c.setg(3,selfaddr);c.setg(4,i&1);c.setg(5,aux+0x7000);c.run(b.entry if compiled else 0x82ba0068);obj=bytearray(c.mem.rd(selfaddr,0x100));obj[:4]=bytes(4);cmd=bytearray(c.mem.rd(aux+0x4000,0x200));got=int.from_bytes(cmd[cursor:cursor+4],'big');expected=(F.address(b,'?StopHandler@SinePlayer@' if i&1 else '?PlayHandler@SinePlayer@') if compiled else (0x82b9b920 if i&1 else 0x82b9b900));assert got==expected;cmd[cursor:cursor+4]=bytes(4);both.append((obj,c.mem.rd(aux+0x1000,0x1200),cmd))
 if both[0]!=both[1]:errors.append({'case':i})
r={'cases':128,'errors':errors,'objects':{p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in [p,q]},'limits':'Actual virtual EventEvent vs ARTIST slot1; raw 8-byte payload, handler checked, vtable/handler pointers normalized. No actual queue scheduling/hardware claim.'};(out/'sine-event-dispatch-comparison.json').write_text(json.dumps(r,indent=2)+'\n');print(len(errors),errors[:1]);assert not errors
