import sys,json,random,struct,hashlib
from pathlib import Path
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave7/dac-terra');sys.path.insert(0,str(out));from weak_bundle import F,WeakBundle
d=out/'vumeter-native-build';objs=[d/'vumeter_probe.obj',d/'VuMeter.obj',d/'TimerHandle.obj'];b=WeakBundle([F.Coff(objs[0],'?NativeVuMeterEventDispatch'),F.Coff(objs[1],'?CreateInstance@VuMeter@'),F.Coff(objs[2],'??0TimerHandle@')]);b.select('?NativeVuMeterEventDispatch');errors=[];rng=random.Random(0x0f80);selfaddr=F.M.S.SELF;aux=F.M.S.AUX;assert F.M.V.x360rd.u32(0x8217f354+4)==0x82ba0f80
for i in range(128):
 img=bytearray(rng.randbytes(0x300));system=bytearray(rng.randbytes(0x1200));ring=bytearray(rng.randbytes(0x200));struct.pack_into('>I',img,4,aux+0x1000);struct.pack_into('>I',system,0x20,aux+0x4000);cursor=(i%5)*4;struct.pack_into('>I',system,0x10b8,cursor);both=[]
 for compiled in (False,True):
  c=F.cpu(b);c.mem.wr(selfaddr,img);c.mem.wr(aux+0x1000,system);c.mem.wr(aux+0x4000,ring)
  if not compiled:c.setg(3,selfaddr+0x24);c.run(0x82b6bea8);F.put(c,selfaddr,0x8217f354);c.setg(3,selfaddr);c.setg(4,i);c.setg(5,0);c.run(0x82ba0f80)
  else:c.setg(3,selfaddr);c.setg(4,i);c.setg(5,0);c.run(b.entry)
  obj=bytearray(c.mem.rd(selfaddr,0x300));assert F.cstring(c,struct.unpack_from('>I',obj,0x30)[0])=='Unknown';obj[:4]=bytes(4);obj[0x30:0x34]=bytes(4);cmd=bytearray(c.mem.rd(aux+0x4000,0x200));got=struct.unpack_from('>I',cmd,cursor)[0];expected=F.address(b,'?ResetHandler@VuMeter@') if compiled else 0x82b9d130;assert got==expected;cmd[cursor:cursor+4]=bytes(4);both.append((obj,c.mem.rd(aux+0x1000,0x1200),cmd))
 if both[0]!=both[1]:errors.append({'case':i})
r={'cases':128,'errors':errors,'objects':{p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in objs},'limits':'Actual VuMeter virtual EventEvent and command queue vs ARTIST; handler identity checked, vtable/Unknown pointers normalized; no command execution/concurrency/hardware claim.'};(out/'vumeter-event-dispatch-comparison.json').write_text(json.dumps(r,indent=2)+'\n');print(len(errors),errors[:1]);assert not errors
