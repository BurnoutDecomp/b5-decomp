import sys,json,random,hashlib,struct
from pathlib import Path
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave7/dac-terra');sys.path.insert(0,str(out));from weak_bundle import F,WeakBundle
d=out/'snd9-native-build';objs=[d/'snd9_probe.obj',d/'Snd9Service.obj',d/'TimerHandle.obj'];b=WeakBundle([F.Coff(objs[0],'?NativeSnd9ServiceLifetime'),F.Coff(objs[1],'?CreateInstance@Snd9Service@'),F.Coff(objs[2],'??0TimerHandle@')]);b.select('?NativeSnd9ServiceLifetime');weak=next(x for x in b.weak_resolved if x[0].startswith('??_ESnd9Service@'));basevts={b.bases[(oi,sec)]+v for oi,o in enumerate(b.objects) for n,v,sec in o.symbols.values() if n.startswith('??_7PlugIn@') and (oi,sec) in b.bases};errors=[];rng=random.Random(0x00d0);selfaddr=F.M.S.SELF
for i in range(128):
 initial=bytearray(rng.randbytes(0x200));both=[]
 for compiled in (False,True):
  c=F.cpu(b);c.mem.wr(selfaddr,initial);trace=[]
  def free(x):trace.append(x.g(3)&0xffffffff)
  c.hooks[0x82c08fb0]=free
  for at,n in b.unresolved.items():
   if n.startswith('??3@YAXPAX'):c.hooks[at]=free
  if not compiled:c.setg(3,selfaddr+0x24);c.run(0x82b6bea8);F.put(c,selfaddr,0x8217f384);c.setg(3,selfaddr);c.setg(4,i&1);c.run(0x82ba00d0)
  else:c.setg(3,selfaddr);c.setg(4,i&1);c.run(b.entry)
  memory=bytearray(c.mem.rd(selfaddr,0x200));got=int.from_bytes(memory[:4],'big');assert got in (basevts if compiled else {0x820aa810});assert F.cstring(c,struct.unpack_from('>I',memory,0x30)[0])=='Unknown';memory[:4]=bytes(4);memory[0x30:0x34]=bytes(4);both.append((memory,trace))
 if both[0]!=both[1]:errors.append({'case':i,'trace':[both[0][1],both[1][1]]})
r={'cases':128,'errors':errors,'weak_fallback':weak,'objects':{p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in objs},'limits':'Actual constructed Snd9 scalar dtor flags 0/1; original TimerHandle ctor + vtable explicitly run, operator delete controlled; vtable and Unknown normalized. No heap/hardware claim.'};(out/'snd9-lifetime-comparison.json').write_text(json.dumps(r,indent=2)+'\n');print(len(errors),errors[:1]);assert not errors
