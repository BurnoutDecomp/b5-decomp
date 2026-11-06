import sys,json,random,struct,hashlib
from pathlib import Path
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave7/dac-terra');sys.path.insert(0,str(out));from weak_bundle import F,WeakBundle
d=out/'reverb-native-build';objs=[d/'reverb_probe.obj',d/'ReverbModel1.obj',d/'ReverbFilters.obj',d/'TimerHandle.obj',d/'DelayLine.obj'];b=WeakBundle([F.Coff(objs[0],'?NativeReverbTickDispatch'),F.Coff(objs[1],'??0ReverbModel1@'),F.Coff(objs[2],'??0AllPassFilter@'),F.Coff(objs[3],'??0TimerHandle@'),F.Coff(objs[4],'??0DelayLine@')]);b.select('?NativeReverbTickDispatch');errors=[];rng=random.Random(0xe9d8);selfaddr=F.M.S.SELF;assert F.M.V.x360rd.u32(0x8217f59c+8)==0x82b9e9d8
for i in range(128):
 img=bytearray(rng.randbytes(0x600));ticks=rng.getrandbits(32);both=[]
 for compiled in (False,True):
  c=F.cpu(b);c.mem.wr(selfaddr,img);c.setg(3,selfaddr)
  if not compiled:c.run(0x82b9e938);F.put(c,selfaddr+0x14c,ticks);c.setg(3,selfaddr);c.run(0x82b9e9d8)
  else:c.setg(4,ticks);c.run(b.entry)
  memory=bytearray(c.mem.rd(selfaddr,0x600));assert F.cstring(c,struct.unpack_from('>I',memory,0x148)[0])=='Unknown';memory[:4]=bytes(4);memory[0x148:0x14c]=bytes(4);both.append((memory,c.g(3)&0xffffffff))
 if both[0]!=both[1]:errors.append({'case':i,'returns':[both[0][1],both[1][1]]})
r={'cases':128,'errors':errors,'objects':{p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in objs},'limits':'Actual constructed Reverb→virtual GetPpuTicksEvent vs original constructor+slot2 method; original vtable slot checked; vtable and equal Unknown pointers normalized, controlled ticks. No hardware claim.'};(out/'reverb-ticks-dispatch-comparison.json').write_text(json.dumps(r,indent=2)+'\n');print(len(errors),errors[:1]);assert not errors
