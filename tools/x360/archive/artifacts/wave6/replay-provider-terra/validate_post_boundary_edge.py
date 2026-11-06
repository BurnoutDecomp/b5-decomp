import sys,json,random,struct,hashlib
from pathlib import Path
root=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(root/'scratch/x360_sdk'));import native_atomic_fixture as F
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave6/replay-provider-terra');r=json.loads((out/'native/compile-results.json').read_text());p={Path(x['source']).name:Path(x['object']) for x in r['results'] if x['passed']};b=F.Bundle([F.Coff(p['eathread_semaphore.cpp'],'?Post@Semaphore@'),F.Coff(p['eathread_semaphore_data.cpp'],'?UpdateCancelCount@EASemaphoreData@')]);b.select('?Post@Semaphore@');assert F.M.V.x360rd.u32(0x82815570)==0x7c8a2378
selfaddr=F.M.S.SELF;errors=[];rng=random.Random(82642);cases=0
for intra in (0,1):
 for count in (-2147483648,-9,-2,-1,0,1,8,2147483647):
  for cancel in (-1,0,1,5):
   for amount in (-2147483648,-2,0,1,3,10,2147483647):
    for osok in (0,1):
     initial=bytearray(rng.randbytes(0x80));struct.pack_into('>IiiB',initial,0,0x4003a000,count,cancel,intra);both=[]
     for compiled in (False,True):
      c=F.cpu(b);c.mem.wr(selfaddr,initial);c.setg(3,selfaddr);c.setg(4,amount & 0xffffffff);trace=[]
      def release(cpu):
       trace.append(tuple(cpu.g(i)&0xffffffff for i in (3,4,5)));cpu.setg(3,osok)
      c.hooks[0x82c204b8]=release
      def cancel_adjust(cpu):
       value=cpu.g(4)&0xffffffff;trace.append(('cancel',value));F.put(cpu,selfaddr+4, -2 if value&1 else 0);F.put(cpu,selfaddr+8, 0)
      c.hooks[0x82b42870]=cancel_adjust
      c.hooks[F.address(b,'?UpdateCancelCount@EASemaphoreData@')]=cancel_adjust
      for addr,name in b.unresolved.items():
       if name=='ReleaseSemaphore':c.hooks[addr]=release
      c.run(b.entry if compiled else 0x82b42b70)
      both.append((c.mem.rd(selfaddr,0x80).hex(),trace,c.g(3)&0xffffffff))
     if both[0]!=both[1]:errors.append({'intra':intra,'count':count,'cancel':cancel,'amount':amount,'osok':osok,'original':both[0],'compiled':both[1]})
     cases+=1
report={'cases':cases,'mismatches':errors,'objects':{k:{'path':str(p[k]),'sha256':hashlib.sha256(p[k].read_bytes()).hexdigest()} for k in ('eathread_semaphore.cpp','eathread_semaphore_data.cpp')},'original':'ARTIST 82B42B70; xref from 82B434D0; calibrated 82815570=7C8A2378','limits':'Actual compiled Post vs original PPC instructions; UpdateCancelCount and ReleaseSemaphore controlled identically, comparing calls, return and object/guard bytes. Existing compiled UpdateCancelCount diverged from original in 18 of 480 composed cases and needs separate investigation. No contention/interleaving injection, kernel execution or physical hardware.'};(out/'post-boundary-edge-comparison.json').write_text(json.dumps(report,indent=2)+'\n');print('cases',cases,'mismatches',len(errors));
if errors:print(json.dumps(errors[0],indent=2))
assert not errors
