import sys,json,random,struct,hashlib
from pathlib import Path
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave7/dac-terra');sys.path.insert(0,str(out));from weak_bundle import F,WeakBundle
d=out/'delay-native-build';objs=[d/'delay_probe.obj',d/'Voice.obj',d/'Delay.obj',d/'DelayFilter.obj',d/'TimerHandle.obj',d/'DelayLine.obj'];b=WeakBundle([F.Coff(objs[0],'?NativeVoiceDelayRelease'),F.Coff(objs[1],'?ReleaseImmediate@Voice@'),F.Coff(objs[2],'??0Delay@'),F.Coff(objs[3],'??0DelayFilter@'),F.Coff(objs[4],'??0TimerHandle@'),F.Coff(objs[5],'??0DelayLine@')]);b.select('?NativeVoiceDelayRelease');errors=[];rng=random.Random(0xdf38);selfaddr=F.M.S.SELF;aux=F.M.S.AUX;stage=aux+0x8000;system=aux+0x2000;allocator=aux+0x4000;vtable=aux+0x5000;freeaddr=aux+0x6000
for i in range(64):
 voice=bytearray(rng.randbytes(0x100));img=bytearray(rng.randbytes(0x600));struct.pack_into('>I',voice,0x10,system);voice[0x44]=1;struct.pack_into('>I',voice,0x4c,stage);both=[]
 for compiled in (False,True):
  c=F.cpu(b);c.mem.wr(selfaddr,voice);c.mem.wr(stage,img);F.put(c,system+0x14,allocator);F.put(c,allocator,vtable);F.put(c,vtable+0xc,freeaddr);trace=[]
  def release(x):trace.append(('delay-release',x.g(3)&0xffffffff))
  def free(x):trace.append(('allocator-free',x.g(3)&0xffffffff,x.g(4)&0xffffffff,x.g(5)&0xffffffff))
  c.hooks[0x82b6cc08]=release;c.hooks[F.address(b,'?Release@DelayLine@')]=release;c.hooks[freeaddr]=free
  if compiled:c.setg(3,selfaddr);c.setg(4,stage);c.setg(5,i%4);c.run(b.entry)
  else:c.setg(3,stage);c.run(0x82b9dd90);c.mem.wr(stage+0xb0,bytes([0]));c.setg(3,selfaddr);c.setg(4,1);c.run(0x82b6df38)
  obj=bytearray(c.mem.rd(stage,0x600));assert F.cstring(c,struct.unpack_from('>I',obj,0xa4)[0])=='Unknown';obj[:4]=bytes(4);obj[0xa4:0xa8]=bytes(4);both.append((c.mem.rd(selfaddr,0x100),obj,trace))
 if both[0]!=both[1]:errors.append({'case':i,'voice_diff':[hex(j) for j,(a,z) in enumerate(zip(both[0][0],both[1][0])) if a!=z][:15],'stage_diff':[hex(j) for j,(a,z) in enumerate(zip(both[0][1],both[1][1])) if a!=z][:15], 'trace':[both[0][2],both[1][2]]})
r={'cases':64,'errors':errors,'objects':{p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in objs},'limits':'Actual Voice::ReleaseImmediate virtual Delay ReleaseEvent and scalar destructor after real constructor, keepInList=1; DelayLine::Release and allocator Free controlled. Vtable/Unknown identities normalized; no real allocator, concurrency, all-stage or hardware claim.'};(out/'voice-delay-release-comparison.json').write_text(json.dumps(r,indent=2)+'\n');print(len(errors),errors[:1]);assert not errors
