from pathlib import Path
import json,random,sys,hashlib
P=Path(r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow');sys.path.insert(0,str(P/'scratch/x360_sdk'));import native_atomic_fixture as F
D=Path(__file__).parent;rs=json.loads((D/'compile-results.json').read_text())['results'];r=next(x for x in rs if x['source'].endswith('BrnOnlineGameRoomPlayerInfo_wH_12.cpp'));obj=Path(r['object']);sym='?HandleSplashScreenRequests@OnlineGameRoomPlayerInfo@BrnGui@@';b=F.Bundle([F.Coff(obj,sym)]);b.select(sym);base=F.M.S.SELF;aux=F.M.S.AUX;cache=0x86000000;state=0x86100000;rng=random.Random(401)
for i in range(40):
 event=[0,0,0,0,1,2,3,0][i%8];mode=[-1,15,16,0,10][i%5];hascache=(i%7)!=0;latch=1 if event==1 else i%2;raw=rng.randbytes(0x15620);outs=[]
 for native in [False,True]:
  c=F.cpu(b);c.mem.add(0x82f31000,0x1000);F.put(c,0x82f31908,0,8);c.mem.add(cache,0x10000);c.mem.add(state,0x1000);c.mem.wr(base,raw);F.put(c,base+0x1c,state);F.put(c,base+0x155dc,cache if hascache else 0);F.put(c,cache+0x9e58,mode);c.mem.wr(base+0x155ed,bytes([latch]));F.put(c,aux,event);trace=[]
  def add(x):
   q=x.g(3)&0xffffffff;p=x.g(4)&0xffffffff;typ=x.g(5)&0xffffffff;size=x.g(6)&0xffffffff;trace.append(('add',q,typ,size,x.mem.rd(p,size).hex()));x.setg(3,1)
  def show(x):trace.append(('show',x.g(3)&0xffffffff))
  def out(x):x.setg(3,(x.g(3)+12)&0xffffffff)
  def lobby(x):x.setg(3,1 if (x.g(3)&0xffffffff) in [15,16] else 0)
  c.hooks[0x82435188]=add;c.hooks[0x82499f00]=show;c.hooks[F.address(b,'?AddEvent@?$VariableEventQueue@$0BAAAA@$0BA@@CgsModule@@')]=add
  for a,n in b.unresolved.items():
   if 'GetOutputEventQueue@StateInterface' in n:c.hooks[a]=out
   elif 'ShowSplashScreen@OnlineGameRoom' in n:c.hooks[a]=show
   elif 'IsOnlineFreeBurnLobby' in n:c.hooks[a]=lobby
  c.setg(3,base);c.setg(4,aux)
  try:c.run(b.entry if native else 0x824a54b8)
  except Exception as e: print('failure',i,native,event,mode,hascache,latch,hex(c.last_pc),repr(e));raise
  outs.append((c.mem.rd(base,0x15620),trace))
 assert outs[0]==outs[1],(i,event,mode,hascache,latch,outs[0][1],outs[1][1],[(hex(k),a,z) for k,(a,z) in enumerate(zip(outs[0][0],outs[1][0])) if a!=z][:10])
report={'passed':True,'cases':40,'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'Actual HandleSplashScreenRequests vs ARTIST824A54B8 for show, stop-while-latched and ignored states; valid cache/modes; actual original 82493880 wrapper executes; queue and ShowSplashScreen controlled. Other stop branch, asserts, complete queue/runtime excluded.'};(D/'splash-comparison.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
