import sys,json,random,struct,hashlib
from pathlib import Path
root=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(root/'scratch/x360_sdk'));import native_atomic_fixture as F
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave7/dac-terra');d=out/'reverb-native-build';objs=[d/'ReverbModel1.obj',d/'ReverbFilters.obj',d/'TimerHandle.obj',d/'DelayLine.obj'];prefix='?CreateInstance@ReverbModel1@';b=F.Bundle([F.Coff(objs[0],prefix),F.Coff(objs[1],'??0AllPassFilter@'),F.Coff(objs[2],'??0TimerHandle@'),F.Coff(objs[3],'??0DelayLine@')]);b.select(prefix);errors=[];rng=random.Random(0x67f8);selfaddr=F.M.S.SELF;aux=F.M.S.AUX
for i in range(128):
 img=bytearray(rng.randbytes(0x600));img[0x21]=[1,2,4,6][i%4];struct.pack_into('>I',img,4,aux+0x1000);both=[]
 for compiled in (False,True):
  c=F.cpu(b);c.mem.wr(selfaddr,img);c.setg(3,selfaddr);trace=[]
  def init(x):trace.append(('init',*[x.g(z)&0xffffffff for z in (3,4,5,6)]));x.setg(3,1)
  def timer(x):
   callback=x.g(5)&0xffffffff;expected=F.address(b,'?RwacTimerClient@ReverbModel1@') if compiled else 0x82ba5720;assert callback==expected,(hex(callback),hex(expected));trace.append(('timer',*[x.g(z)&0xffffffff for z in (3,4,6,8,9)],F.cstring(x,x.g(7)&0xffffffff)));x.setg(3,i&1)
  c.hooks[0x82b6c968]=init;c.hooks[F.address(b,'?Init@DelayLine@')]=init;c.hooks[0x82b6eb88]=timer
  for at,n in b.unresolved.items():
   if n.startswith('?AddTimer@TimerManager@'):c.hooks[at]=timer
   if n.startswith(('__savefpr_', '__restfpr_')):c.hooks[at]=lambda x:None
  c.run(b.entry if compiled else 0x82ba67f8);obj=bytearray(c.mem.rd(selfaddr,0x600));assert F.cstring(c,struct.unpack_from('>I',obj,0x148)[0])=='Unknown';obj[:4]=bytes(4);obj[0x148:0x14c]=bytes(4);both.append((obj,trace,c.g(3)&0xffffffff))
 if both[0]!=both[1]:errors.append({'case':i,'diff':[hex(j) for j,(a,z) in enumerate(zip(both[0][0],both[1][0])) if a!=z][:32],'trace':[both[0][1],both[1][1]],'return':[both[0][2],both[1][2]]})
r={'cases':128,'errors':errors,'objects':{p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in objs},'limits':'Actual Reverb CreateInstance and all real nested constructors; DelayLine::Init and AddTimer controlled, callback identities verified, vtable and equal Unknown strings normalized; modes 1,2,4,6, both timer outcomes; no heap/concurrency/hardware claim.'};(out/'reverb-create-comparison.json').write_text(json.dumps(r,indent=2)+'\n');print(len(errors),errors[:1]);assert not errors
