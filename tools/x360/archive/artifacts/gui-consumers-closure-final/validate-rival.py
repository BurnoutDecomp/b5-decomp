from pathlib import Path
import json,random,sys,hashlib
P=Path(r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow');sys.path.insert(0,str(P/'scratch/x360_sdk'));import native_atomic_fixture as F
D=Path(__file__).parent;r=json.loads((D/'compile-results.json').read_text())['results'][0];obj=Path(r['object']);sym='?GetCrashNavIconStateForRival@MapIconManager@BrnGui@@';b=F.Bundle([F.Coff(obj,sym)]);b.select(sym);base=F.M.S.SELF;aux=F.M.S.AUX;cache=0x86000000;rng=random.Random(331)
for i in range(72):
 icon=bytearray(rng.randbytes(64));icon[0x28]=3 if i%9==0 else 2;target=5;slot=i%9;team=(i//3)%3;local=(i//9)%2;outs=[]
 for native in [False,True]:
  c=F.cpu(b);c.mem.add(cache,0x20000);c.mem.wr(aux,icon);F.put(c,base+0xa9f8,cache);F.put(c,cache+0x9e58,0)
  for n in range(8):
   rec=cache+0xac80+n*312;F.put(c,rec+0x114,target if n==slot else 20+n);c.mem.wr(rec+0x12f,bytes([local]));c.mem.wr(rec+0x12c,b'\x01')
  trace=[]
  def ty(x):x.setg(3,x.mem.rd((x.g(3)&0xffffffff)+0x28,1)[0])
  def car(x):x.setg(3,target)
  def player(x):x.setg(3,cache+0xac80+312*x.g(4))
  def t(x):trace.append(('team',x.g(4)));x.setg(3,team)
  c.hooks.update({0x823a6b30:ty,0x824b2ef8:car,0x8240f910:t})
  for a,n in b.unresolved.items():
   if 'GetActiveRaceCarIndex' in n:c.hooks[a]=car
   elif 'GetOnlinePlayerInfo@GuiCache' in n:c.hooks[a]=player
   elif 'GetCurrentOnlinePlayerTeam' in n:c.hooks[a]=t
  c.setg(3,base);c.setg(4,aux);c.run(b.entry if native else 0x824f4680);outs.append((c.g(3)&0xffffffff,trace))
 assert outs[0]==outs[1],(i,slot,team,local,outs)
report={'passed':True,'cases':72,'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'Actual GetCrashNavIconStateForRival vs ARTIST824F4680 on valid rival/network types, 8 slots or absent, local visibility and three teams; icon accessor, typed cache lookup and team query controlled. Online-colour modes and assert paths excluded.'};(D/'rival-comparison.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
