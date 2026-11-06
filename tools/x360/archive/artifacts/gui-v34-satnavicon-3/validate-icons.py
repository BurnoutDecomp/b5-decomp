from pathlib import Path
import json,random,sys,hashlib,struct
P=Path(r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow');sys.path.insert(0,str(P/'scratch/x360_sdk'));import native_atomic_fixture as F
D=Path(__file__).parent;obj=Path(next(x for x in json.loads((D/'compile-results.json').read_text())['results'] if x['source']=='native_icon_v34_probe.cpp')['object']);base=F.M.S.SELF;rng=random.Random(800);cases=0
for prefix,orig in [('?ProbeCrash@@',0x827dd568),('?ProbeSat@@',0x827dd7c8)]:
 b=F.Bundle([F.Coff(obj,prefix)]);b.select(prefix)
 for i in range(96):
  old=[rng.getrandbits(32) for _ in range(4)];new=[rng.getrandbits(32) for _ in range(4)]
  if i%4==0:new[:2]=old[:2]
  if i%4==1:old[0]=0;new[0]=0x80000000;old[1]=new[1]=0x3f800000
  if i%4==2:old[0]=new[0]=0x7fc00001;old[1]=new[1]=0x3f800000
  raw=bytearray(rng.randbytes(0x200));raw[0x10:0x20]=struct.pack('>4I',*old);outs=[]
  for native in [False,True]:
   c=F.cpu(b);c.mem.wr(base,raw);trace=[]
   def cb(x):trace.append((x.g(3)&0xffffffff,tuple(x.vr[1])))
   c.hooks[0x8241dfc0]=cb
   for a,n in b.unresolved.items():
    if 'SetPosition@MovieClipRef' in n:c.hooks[a]=cb
   c.setg(3,base);c.vr[1]=tuple(new);c.run(b.entry if native else orig);outs.append((c.mem.rd(base+0x10,16),c.mem.rd(base+0x15c,1) if prefix=='?ProbeCrash@@' else b'',trace,raw[0x15c]))
  assert outs[0][:3]==outs[1][:3],(prefix,i,old,new,outs)
  cases+=1
report={'passed':True,'cases':cases,'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'Actual SDK-ABI probes vs ARTIST 827DD568/827DD7C8. Incoming position passed in v1, XY equal/changed, signed zero and qNaN, unequal tails; whole vector stores and SatNav callback examined. Dirty byte initial state randomized and compared. VMX emulator model, no hardware NaN/denormal guarantee.'};(D/'icon-comparison.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
