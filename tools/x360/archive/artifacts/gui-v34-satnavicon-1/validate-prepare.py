from pathlib import Path
import json,random,sys,hashlib
P=Path(r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow');sys.path.insert(0,str(P/'scratch/x360_sdk'));import native_atomic_fixture as F
D=Path(__file__).parent;obj=Path(json.loads((D/'compile-results.json').read_text())['results'][0]['object']);sym='?Prepare@SatNavIconComponent@BrnGui@@';b=F.Bundle([F.Coff(obj,sym)]);b.select(sym);base=F.M.S.SELF;aux=F.M.S.AUX;table=0x86100000;rng=random.Random(774)
for i in range(32):
 raw=rng.randbytes(0x200);outs=[]
 for native in [False,True]:
  c=F.cpu(b);c.mem.add(table,0x1000);c.mem.wr(base,raw);F.put(c,base+0x20,table);trace=[]
  def prep(x):trace.append(('prepare',tuple(x.g(n)&0xffffffff for n in [3,4,5,6])))
  def cb(name):
   def call(x):trace.append((name,x.g(3)&0xffffffff,tuple(x.vr[1]) if name=='pos' else x.f(1) if name in ('rot','alpha') else x.g(4)&0xffffffff))
   return call
  c.hooks[0x8241c250]=prep
  for offset,name in [(4,'pos'),(8,'rot'),(12,'alpha'),(16,'state')]:
   addr=0x84801000+offset*4;F.put(c,table+offset,addr);c.hooks[addr]=cb(name)
  for a,n in b.unresolved.items():
   if 'Prepare@FlaptIconComponent' in n:c.hooks[a]=prep
  c.setg(3,base);c.setg(4,aux);c.setg(5,aux+0x100)
  try:c.run(b.entry if native else 0x82448488)
  except Exception as e: print('FAIL',i,native,trace,hex(c.last_pc),repr(e));raise
  outs.append((c.mem.rd(base,len(raw)),trace))
 assert outs[0]==outs[1],(i,outs[0][1],outs[1][1],[(hex(k),a,z) for k,(a,z) in enumerate(zip(outs[0][0],outs[1][0])) if a!=z][:10])
report={'passed':True,'cases':32,'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'Actual SatNavIconComponent::Prepare vs ARTIST82448488, randomized component bytes, original virtual call order/arguments including all-zero v1 compared; FlaptIcon and virtual methods controlled. Full runtime excluded.'};(D/'satnav-prepare-comparison.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
