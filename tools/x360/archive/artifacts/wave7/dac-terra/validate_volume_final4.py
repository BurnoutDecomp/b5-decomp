import sys,json,random,struct,hashlib
from pathlib import Path
root=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(root/'scratch/x360_sdk'));import native_atomic_fixture as F
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave7/dac-terra');report=json.loads((out/'build-final4/compile-results.json').read_text());p=Path(report['results'][0]['object']);selfaddr=F.M.S.SELF;aux=F.M.S.AUX;errors=[];rng=random.Random(0x96dc8)
for kind,original,prefix in [('start',0x82b96dc8,'?StartImmediate@Dac@'),('stop',0x82b96e38,'?StopImmediate@Dac@')]:
 b=F.Bundle([F.Coff(p,prefix)]);b.select(prefix)
 for i in range(128):
  image=bytearray(rng.randbytes(0x3200));struct.pack_into('>I',image,0x40,0 if i%4==0 else aux+0x800);flag=i&1;external=(i//2)&1;ramp=(i//4)&1;both=[]
  for compiled in (False,True):
   c=F.cpu(b);c.mem.add(0x8327a000,0x5000);c.mem.wr(selfaddr,image);c.setg(3,selfaddr);started=F.address(b,'sStarted') if compiled else 0x8327a587;ext=F.address(b,'sExternalDacMode') if compiled else 0x8327ee58;req=F.address(b,'sStartRequested') if compiled else 0x8327a584;c.mem.wr(started,bytes([flag]));F.put(c,ext,external);c.mem.wr(req,bytes([ramp]));trace=[]
   def volume(x):trace.append((x.g(3)&0xffffffff,x.f(1)))
   c.hooks[0x82c2d1f0]=volume
   for at,name in b.unresolved.items():
    if name=='XAudioSourceVoice_SetVolume':c.hooks[at]=volume
   c.run(b.entry if compiled else original);both.append((c.mem.rd(selfaddr,0x3200),c.mem.rd(started,1),c.mem.rd(req,1),trace))
  if both[0]!=both[1]:errors.append({'kind':kind,'case':i,'original_trace':both[0][3],'compiled_trace':both[1][3]})
r={'cases':256,'mismatches':errors,'original':['82B96DC8','82B96E38'],'object':str(p),'sha256':hashlib.sha256(p.read_bytes()).hexdigest(),'limits':'Native StartImmediate/StopImmediate state stores and controlled volume call compared, not undefined return-register residue, XAudio device or hardware.'};(out/'volume-final4-comparison.json').write_text(json.dumps(r,indent=2)+'\n');print('256 cases',len(errors),'mismatches');print(errors[:1]);assert not errors
