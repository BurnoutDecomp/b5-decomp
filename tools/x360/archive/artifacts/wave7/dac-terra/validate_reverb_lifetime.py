import sys,json,random,hashlib,struct
from pathlib import Path
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave7/dac-terra');sys.path.insert(0,str(out));from weak_bundle import F,WeakBundle
d=out/'reverb-native-build';objs=[d/'reverb_probe.obj',d/'ReverbModel1.obj',d/'ReverbFilters.obj',d/'TimerHandle.obj',d/'DelayLine.obj'];b=WeakBundle([F.Coff(objs[0],'?NativeReverbModel1Lifetime'),F.Coff(objs[1],'??0ReverbModel1@'),F.Coff(objs[2],'??0AllPassFilter@'),F.Coff(objs[3],'??0TimerHandle@'),F.Coff(objs[4],'??0DelayLine@')]);b.select('?NativeReverbModel1Lifetime');weak=next(x for x in b.weak_resolved if x[0].startswith('??_EReverbModel1@'));basevts={b.bases[(oi,sec)]+v for oi,o in enumerate(b.objects) for n,v,sec in o.symbols.values() if n.startswith('??_7PlugIn@') and (oi,sec) in b.bases};errors=[];rng=random.Random(0xea48);selfaddr=F.M.S.SELF
for i in range(128):
 initial=bytearray(rng.randbytes(0x600));both=[]
 for compiled in (False,True):
  c=F.cpu(b);c.mem.wr(selfaddr,initial);trace=[]
  def release(x):trace.append(('delay-release',x.g(3)&0xffffffff))
  def free(x):trace.append(('delete',x.g(3)&0xffffffff))
  c.hooks[0x82b6cc08]=release;c.hooks[F.address(b,'?Release@DelayLine@')]=release;c.hooks[0x82c08fb0]=free
  for at,n in b.unresolved.items():
   if n.startswith('??3@YAXPAX'):c.hooks[at]=free
  c.setg(3,selfaddr)
  if not compiled:c.run(0x82b9e938);c.setg(3,selfaddr);c.setg(4,i&1);c.run(0x82b9ea48)
  else:c.setg(4,i&1);c.run(b.entry)
  memory=bytearray(c.mem.rd(selfaddr,0x600));got=int.from_bytes(memory[:4],'big');assert got in (basevts if compiled else {0x820aa810});assert F.cstring(c,struct.unpack_from('>I',memory,0x148)[0])=='Unknown';memory[:4]=bytes(4);memory[0x148:0x14c]=bytes(4);both.append((memory,trace))
 if both[0]!=both[1]:errors.append({'case':i,'diff':[hex(j) for j,(a,z) in enumerate(zip(both[0][0],both[1][0])) if a!=z][:24],'trace':[both[0][1],both[1][1]]})
r={'cases':128,'errors':errors,'weak_fallback':weak,'objects':{p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in objs},'limits':'Actual constructed Reverb scalar-destructor paths flags 0/1 with real nested DelayLine destructors; DelayLine::Release and operator delete controlled. Base vtable and equal Unknown pointer verified/normalized; no heap/hardware claim.'};(out/'reverb-lifetime-comparison.json').write_text(json.dumps(r,indent=2)+'\n');print(len(errors),errors[:1]);assert not errors
