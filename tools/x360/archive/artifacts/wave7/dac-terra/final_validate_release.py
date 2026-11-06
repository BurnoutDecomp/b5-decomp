import sys,json,random,struct,hashlib
from pathlib import Path
root=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(root/'scratch/x360_sdk'));import native_atomic_fixture as F
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave7/dac-terra');report=json.loads((out/'abi-final/compile-results.json').read_text());p=Path(report['results'][0]['object']);b=F.Bundle([F.Coff(p,'?ReleaseEvent@Dac@')]);b.select('?ReleaseEvent@Dac@');selfaddr=F.M.S.SELF;aux=F.M.S.AUX;errors=[];rng=random.Random(0xdae0)
orig={'release':0x82c2cea8,'eventfree':0x82c2d508,'shutdown':0x82c2d340,'threadid':0x82b6bcb8,'free':0x82b6be48,'volume':0x82c2d1f0};names={'release':'XAudioVoice_Release','eventfree':'XAudioFreeEvent','shutdown':'XAudioShutDown','threadid':'?SetRwAudioCoreThreadId@System@','free':'?Free@System@','volume':'XAudioSourceVoice_SetVolume'}
for i in range(128):
 img=bytearray(rng.randbytes(0x3200));struct.pack_into('>I',img,4,aux+0x100);struct.pack_into('>I',img,0x24,0 if i&4 else aux+0x900);struct.pack_into('>I',img,0x40,0 if i&8 else aux+0x800);struct.pack_into('>I',img,0x304c,0 if i&16 else aux+0x700);external=i&1;started=(i>>1)&1;both=[]
 for compiled in (False,True):
  c=F.cpu(b);c.mem.add(0x8327a000,0x5000);c.mem.wr(selfaddr,img);c.setg(3,selfaddr);running=F.address(b,'sThreadRunning') if compiled else 0x8327a588;start=F.address(b,'sStarted') if compiled else 0x8327a587;ext=F.address(b,'sExternalDacMode') if compiled else 0x8327ee58;c.mem.wr(running,b'\x01');c.mem.wr(start,bytes([started]));F.put(c,ext,external);trace=[]
  def make(k):
   def call(x):
    a=[x.g(j)&0xffffffff for j in (3,4,5)]
    if k=='threadid':trace.append((k,a[0],int.from_bytes(x.mem.rd(a[1],4),'big')))
    elif k=='free':trace.append((k,*a))
    elif k=='volume':trace.append((k,a[0],x.f(1)))
    elif k=='shutdown':trace.append((k,))
    else:trace.append((k,a[0]))
   return call
  for k,addr in orig.items():c.hooks[addr]=make(k)
  for addr,name in b.unresolved.items():
   for k,prefix in names.items():
    if name.startswith(prefix):c.hooks[addr]=make(k)
  c.run(b.entry if compiled else 0x82b9dae0);both.append((c.mem.rd(selfaddr,0x3200),c.mem.rd(running,1),c.mem.rd(start,1),trace))
 if both[0]!=both[1]:errors.append({'case':i,'orig_trace':both[0][3],'compiled_trace':both[1][3]})
r={'cases':128,'mismatches':errors,'original':'82B9DAE0','object':str(p),'sha256':hashlib.sha256(p.read_bytes()).hexdigest(),'limits':'Actual compiled ReleaseEvent and nested StopImmediate versus original PPC. XAudio and System leaves controlled; object/guard bytes, flags, trace/arguments compared. No thread synchronization, physical hardware, kernel, actual mixer-free or deleting-destructor provider proof.'};(out/'release-comparison-abi-final.json').write_text(json.dumps(r,indent=2)+'\n');print('128 cases',len(errors),'mismatches');print(errors[:1]);assert not errors
