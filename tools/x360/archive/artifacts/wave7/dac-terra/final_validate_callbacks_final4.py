import sys,json,random,struct,hashlib
from pathlib import Path
root=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(root/'scratch/x360_sdk'));import native_atomic_fixture as F
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave7/dac-terra');report=json.loads((out/'abi-final/compile-results.json').read_text());p=Path(report['results'][0]['object']);assert F.M.V.x360rd.u32(0x82815570)==0x7c8a2378
selfaddr=F.M.S.SELF;args=F.M.S.AUX;errors=[];rng=random.Random(0x97208);cases=0
for kind,original,prefix in [('process',0x82b97208,'?XenonProcessCb@Dac@'),('packet',0x82b97228,'?XenonPacketCompleteCb@Dac@')]:
 b=F.Bundle([F.Coff(p,prefix)]);b.select(prefix)
 for i in range(128):
  image=bytearray(rng.randbytes(0x3200));callback=bytearray(rng.randbytes(0x40));event=0 if i%4==0 else 0x4003a000;struct.pack_into('>I',image,0x304c,event);struct.pack_into('>II',callback,0,selfaddr,selfaddr+0x3100);active=i&1;both=[]
  for compiled in (False,True):
   c=F.cpu(b);c.mem.wr(selfaddr,image);c.mem.wr(args,callback);c.setg(3,args);trace=[]
   def signal(cpu):trace.append(cpu.g(3)&0xffffffff)
   c.hooks[0x82c2d530]=signal
   for at,name in b.unresolved.items():
    if name=='XAudioSetEvent':c.hooks[at]=signal
   if kind=='process':
    c.mem.add(0x8327a000,0x1000);c.mem.wr(0x8327a588,bytes([active]));c.mem.wr(F.address(b,'sThreadRunning'),bytes([active]))
   c.run(b.entry if compiled else original);both.append((c.mem.rd(selfaddr,0x3200),c.mem.rd(args,0x40),trace))
  if both[0]!=both[1]:errors.append({'kind':kind,'case':i,'original_calls':both[0][2],'compiled_calls':both[1][2]})
  cases+=1
r={'cases':cases,'mismatches':errors,'object':str(p),'sha256':hashlib.sha256(p.read_bytes()).hexdigest(),'original':['82B97208','82B97228'],'limits':'Actual native callback bodies versus original PPC; XAudioSetEvent controlled, full DAC/callback memory, event calls and conditions compared. No kernel event, voice playback, concurrency or hardware execution.'};(out/'callbacks-final4-comparison-abi-final.json').write_text(json.dumps(r,indent=2)+'\n');print(cases,'cases',len(errors),'mismatches');print(errors[:1]);assert not errors
