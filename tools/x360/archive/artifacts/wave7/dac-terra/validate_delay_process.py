import sys,json,random,struct,hashlib
from pathlib import Path
root=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(root/'scratch/x360_sdk'));import native_atomic_fixture as F
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave7/dac-terra');d=out/'delay-native-build';objs=[d/'Delay.obj',d/'DelayFilter.obj',d/'DelayLine.obj',d/'TimerHandle.obj'];prefix='?Process@Delay@';b=F.Bundle([F.Coff(objs[0],prefix),F.Coff(objs[1],'??0DelayFilter@'),F.Coff(objs[2],'??0DelayLine@'),F.Coff(objs[3],'??0TimerHandle@')]);b.select(prefix);errors=[];rng=random.Random(0x2a08);selfaddr=F.M.S.SELF;aux=F.M.S.AUX;mix=0x50000000
for i in range(128):
 img=bytearray(0x300);struct.pack_into('>I',img,4,aux+0x3000);struct.pack_into('>f',img,0x28,0.01 if i&1 else 0.);struct.pack_into('>f',img,0x30,0.5);struct.pack_into('>I',img,0x38,(i//2)%4);struct.pack_into('>I',img,0x5c+0xc,1000);struct.pack_into('>I',img,0x5c+0x14,1024);struct.pack_into('>I',img,0x5c+0x20,1000 if (i//8)&1 else 100);struct.pack_into('>I',img,0x5c+0x28,128);both=[]
 for compiled in (False,True):
  c=F.cpu(b);c.mem.add(mix,0x31000);c.mem.wr(selfaddr,img);F.put(c,aux+0x3000,aux+0x4000);F.put(c,aux+0x400c,aux+0xf000);F.put(c,aux+0x500c,struct.unpack('>I',struct.pack('>f',48000.))[0]);F.put(c,mix+0x30018,aux+0x5000);F.put(c,mix+0x3000c,aux+0x1000);F.put(c,mix+0x30010,aux+0x2000);c.setg(3,selfaddr);c.setg(4,mix);trace=[]
  def apply(x):trace.append(('apply',*[x.g(z)&0xffffffff for z in (3,4,5,6,7)]))
  def latency(x):trace.append(('latency',x.g(3)&0xffffffff))
  c.hooks[0x82b6e4a0]=apply;c.hooks[F.address(b,'?ApplyFilter@DelayLine@')]=apply;c.hooks[0x82b9de50]=latency;c.hooks[F.address(b,'?UpdateLatencyAndDecay@Delay@')]=latency
  for at,n in b.unresolved.items():
   if n.startswith(('__savefpr_', '__restfpr_')):c.hooks[at]=lambda x:None
  c.run(b.entry if compiled else 0x82ba2a08);obj=bytearray(c.mem.rd(selfaddr,0x300));
  for off,name,orig in [(0x44,'?DelayFilterApplyFunc@DelayFilter@',0x82b67dd0),(0x48,'?DelayFilterResetFunc@DelayFilter@',0x82b67e88)]:
   got=int.from_bytes(obj[off:off+4],'big');expect=F.address(b,name) if compiled else orig;assert got==expect,(hex(got),hex(expect));obj[off:off+4]=bytes(4)
  both.append((obj,c.mem.rd(mix+0x30000,0x40),c.mem.rd(aux+0x4000,0x20),trace,c.g(3)&0xffffffff))
 if both[0]!=both[1]:errors.append({'case':i,'objdiff':[hex(j) for j,(a,z) in enumerate(zip(both[0][0],both[1][0])) if a!=z][:25], 'trace':[both[0][3],both[1][3]]})
r={'cases':128,'errors':errors,'objects':{p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in objs},'limits':'Actual Delay Process and real DelayFilter reset/feedback and DelayLine SetFilter/SetLocalBuffer/GetValid/Reset/SetDelay; ApplyFilter and latency callback controlled; callback identities verified/normalized, finite state cases. No actual ring/hardware claim.'};(out/'delay-process-boundary-comparison.json').write_text(json.dumps(r,indent=2)+'\n');print(len(errors),errors[:1]);assert not errors
