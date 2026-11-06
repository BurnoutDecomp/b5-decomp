from pathlib import Path
import json,random,sys,hashlib
P=Path(r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow');sys.path.insert(0,str(P/'scratch/x360_sdk'));import native_atomic_fixture as F
D=Path(__file__).parent;obj=Path(next(x for x in json.loads((D/'compile-results.json').read_text())['results'] if x['source'].endswith('BrnSatNavIcon.cpp'))['object']);sym='?SetIconText@CrashNavMapIcon@BrnGui@@';b=F.Bundle([F.Coff(obj,sym)]);b.select(sym);base=F.M.S.SELF;aux=F.M.S.AUX;rng=random.Random(731)
for i in range(16):
 text=0 if i%4==0 else aux;flag=i%2;raw=rng.randbytes(0x200);outs=[]
 for native in [False,True]:
  c=F.cpu(b);c.mem.wr(base,raw);c.mem.wr(aux,b'icon_key\0');trace=[]
  def st(x):trace.append(('text',x.g(3)&0xffffffff,F.cstring(x,x.g(4)&0xffffffff)))
  def loc(x):trace.append(('local',x.g(3)&0xffffffff,F.cstring(x,x.g(4)&0xffffffff),x.g(5)&0xffffffff))
  c.hooks[0x824e7240]=st;c.hooks[0x824e7418]=loc
  for a,n in b.unresolved.items():
   if 'SetLocalisedText@TextField' in n:c.hooks[a]=loc
   elif 'SetText@TextField' in n:c.hooks[a]=st
  c.setg(3,base);c.setg(4,text);c.setg(5,flag);c.run(b.entry if native else 0x82448310);outs.append((c.mem.rd(base,0x200),trace))
 assert outs[0]==outs[1],(i,outs[0][1],outs[1][1])
report={'passed':True,'cases':16,'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'Actual CrashNavMapIcon::SetIconText vs ARTIST82448310 for null/non-null and both localization flags; TextField service arguments controlled, full runtime excluded.'};(D/'icon-text-comparison.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
