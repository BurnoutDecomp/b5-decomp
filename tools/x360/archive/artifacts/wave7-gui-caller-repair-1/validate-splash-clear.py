from pathlib import Path
import json,random,sys,hashlib
P=Path(r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow');sys.path.insert(0,str(P/'scratch/x360_sdk'));import native_atomic_fixture as F
D=Path(__file__).parent;rs=json.loads((D/'compile-results.json').read_text())['results'];r=next(x for x in rs if x['source'].endswith('BrnOnlineGameRoomPlayerInfo_wH_12.cpp'));obj=Path(r['object']);sym='?ClearSplashScreenOverlay@OnlineGameRoomPlayerInfo@BrnGui@@';b=F.Bundle([F.Coff(obj,sym)]);b.select(sym);base=F.M.S.SELF;aux=F.M.S.AUX;cache=0x86000000;state=0x86100000;rng=random.Random(311)
for i in range(32):
 mode=[-1,0,15,17][i%4];match=(i//4)%2;leave=(i//8)%2;hv=rng.getrandbits(64);raw=rng.randbytes(0x15620);outs=[]
 for native in [False,True]:
  c=F.cpu(b);c.mem.add(cache,0x10000);c.mem.add(state,0x1000);c.mem.wr(base,raw);F.put(c,base+0x1c,state);F.put(c,base+0x155dc,cache);F.put(c,cache+0x9e58,mode);c.mem.wr(aux,(hv if match else hv^0x100000000).to_bytes(8,'big'));F.put(c,aux+8,leave);trace=[]
  def add(x):
   q=x.g(3)&0xffffffff;p=x.g(4)&0xffffffff;typ=x.g(5)&0xffffffff;size=x.g(6)&0xffffffff;trace.append(('add',q,typ,size,x.mem.rd(p,size).hex()));x.setg(3,1)
  def out(x):x.setg(3,(x.g(3)+12)&0xffffffff)
  def text(x):x.setg(3,aux+0x100)
  def hs(x):x.setg(3,hv)
  c.hooks.update({0x82435188:add,0x824845e8:text,0x82815a20:hs});c.hooks[F.address(b,'?AddEvent@?$VariableEventQueue@$0BAAAA@$0BA@@CgsModule@@')]=add
  for a,n in b.unresolved.items():
   if 'GetOutputEventQueue@StateInterface' in n:c.hooks[a]=out
   elif 'GetSplashScreenIDForGameMode' in n:c.hooks[a]=text
   elif 'CgsIDCompress' in n:c.hooks[a]=hs
  c.setg(3,base);c.setg(4,aux);c.run(b.entry if native else 0x82498e00);outs.append((c.mem.rd(base,0x15620),trace))
 assert outs[0]==outs[1],(i,mode,match,leave,outs[0][1],outs[1][1],[(hex(k),a,z) for k,(a,z) in enumerate(zip(outs[0][0],outs[1][0])) if a!=z][:10])
report={'passed':True,'cases':32,'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'Actual ClearSplashScreenOverlay vs ARTIST82498E00: valid modes, matching/high-word-different overlay ID and leave method; ID services and queue controlled, original 82493880 wrapper executes. Assert and full runtime excluded.'};(D/'splash-clear-comparison.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
