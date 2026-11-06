import sys,json,random,struct,hashlib
from pathlib import Path
root=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(root/'scratch/x360_sdk'));import native_atomic_fixture as F
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave7/dac-terra');d=out/'delay-native-build';objs=[d/'Delay.obj',d/'DelayFilter.obj',d/'DelayLine.obj',d/'TimerHandle.obj'];prefix='?CreateInstance@Delay@';b=F.Bundle([F.Coff(objs[0],prefix),F.Coff(objs[1],'??0DelayFilter@'),F.Coff(objs[2],'??0DelayLine@'),F.Coff(objs[3],'??0TimerHandle@')]);b.select(prefix);errors=[];rng=random.Random(0x2918);selfaddr=F.M.S.SELF;aux=F.M.S.AUX
for i in range(128):
 img=bytearray(rng.randbytes(0x300));img[0x20]=1+i%6;struct.pack_into('>I',img,4,aux+0x1000);both=[]
 for compiled in (False,True):
  c=F.cpu(b);c.mem.wr(selfaddr,img);F.put(c,aux+0x5000,struct.unpack('>I',struct.pack('>f',0.25))[0]);c.setg(3,selfaddr);c.setg(4,aux+0x5000 if i&1 else 0);trace=[]
  def init(x):trace.append(('init',*[x.g(z)&0xffffffff for z in (3,4,5,6)]));x.setg(3,0 if (i//2)%3==0 else 1)
  def timer(x):
   callback=x.g(5)&0xffffffff;expected=F.address(b,'?RwacTimerClient@Delay@') if compiled else 0x82b972c8;assert callback==expected;trace.append(('timer',*[x.g(z)&0xffffffff for z in (3,4,6,8,9)],F.cstring(x,x.g(7)&0xffffffff)));x.setg(3,(i//3)&1)
  c.hooks[0x82b6c968]=init;c.hooks[F.address(b,'?Init@DelayLine@')]=init;c.hooks[0x82b6eb88]=timer
  for at,n in b.unresolved.items():
   if n.startswith('?AddTimer@TimerManager@'):c.hooks[at]=timer
  c.run(b.entry if compiled else 0x82ba2918);obj=bytearray(c.mem.rd(selfaddr,0x300));assert F.cstring(c,struct.unpack_from('>I',obj,0xa4)[0])=='Unknown';obj[:4]=bytes(4);obj[0xa4:0xa8]=bytes(4);both.append((obj,trace,c.g(3)&0xffffffff))
 if both[0]!=both[1]:errors.append({'case':i,'diff':[hex(j) for j,(a,z) in enumerate(zip(both[0][0],both[1][0])) if a!=z][:24],'trace':[both[0][1],both[1][1]],'return':[both[0][2],both[1][2]]})
r={'cases':128,'errors':errors,'objects':{p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in objs},'limits':'Actual Delay CreateInstance and real nested constructors; DelayLine Init/AddTimer controlled, callback identities verified, optional delay config and both outcomes; vtable/Unknown normalized; no heap/hardware claim.'};(out/'delay-create-comparison.json').write_text(json.dumps(r,indent=2)+'\n');print(len(errors),errors[:1]);assert not errors
