import sys,json,random,struct,hashlib
from pathlib import Path
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave7/dac-terra');sys.path.insert(0,str(out));from weak_bundle import F,WeakBundle
d=out/'vumeter-native-build';objs=[d/'vumeter_probe.obj',d/'VuMeter.obj',d/'TimerHandle.obj'];b=WeakBundle([F.Coff(objs[0],'?NativeVuMeterTickDispatch'),F.Coff(objs[1],'?CreateInstance@VuMeter@'),F.Coff(objs[2],'??0TimerHandle@')]);b.select('?NativeVuMeterTickDispatch');errors=[];rng=random.Random(0x6840);selfaddr=F.M.S.SELF;assert F.M.V.x360rd.u32(0x8217f354+8)==0x82b96840
for i in range(128):
 img=bytearray(rng.randbytes(0x200));ticks=rng.getrandbits(32);both=[]
 for compiled in (False,True):
  c=F.cpu(b);c.mem.wr(selfaddr,img)
  if not compiled:c.setg(3,selfaddr+0x24);c.run(0x82b6bea8);F.put(c,selfaddr,0x8217f354);F.put(c,selfaddr+0x34,ticks);c.setg(3,selfaddr);c.run(0x82b96840)
  else:c.setg(3,selfaddr);c.setg(4,ticks);c.run(b.entry)
  memory=bytearray(c.mem.rd(selfaddr,0x200));assert F.cstring(c,struct.unpack_from('>I',memory,0x30)[0])=='Unknown';memory[:4]=bytes(4);memory[0x30:0x34]=bytes(4);both.append((memory,c.g(3)&0xffffffff))
 if both[0]!=both[1]:errors.append({'case':i,'returns':[both[0][1],both[1][1]]})
r={'cases':128,'errors':errors,'objects':{p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in objs},'limits':'Actual Snd9→virtual GetPpuTicksEvent vs original TimerHandle+slot2 method; original vtable slot verified; vtable and Unknown pointer normalized; no hardware claim.'};(out/'vumeter-ticks-comparison.json').write_text(json.dumps(r,indent=2)+'\n');print(len(errors),errors[:1]);assert not errors
