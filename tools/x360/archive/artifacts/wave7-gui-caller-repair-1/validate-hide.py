from pathlib import Path
import json,random,sys,hashlib
P=Path(r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow');sys.path.insert(0,str(P/'scratch/x360_sdk'));import native_atomic_fixture as F
D=Path(__file__).parent;rs=json.loads((D/'compile-results.json').read_text())['results'];r=next(x for x in rs if x['source'].endswith('BrnCrashNavEnterOnline_wI_07.cpp'));obj=Path(r['object']);sym='?HandleShowLoginQuestion@CrashNavEnterOnlineBase@BrnGui@@';b=F.Bundle([F.Coff(obj,sym)]);b.select(sym);base=F.M.S.SELF;aux=F.M.S.AUX;state=0x86100000;callback=0x84801000;rng=random.Random(174)
for i in range(32):
 question=i%7;sign=(i//7)%2;raw=rng.randbytes(0x3800);hv=rng.getrandbits(64);outs=[]
 for native in [False,True]:
  c=F.cpu(b);c.mem.add(state,0x1000);c.mem.wr(base,raw);F.put(c,base+0x1c,state);F.put(c,base+0xcc,sign);F.put(c,base+0xd0+question*4,callback);F.put(c,aux,question);trace=[]
  def add(x):
   q=x.g(3)&0xffffffff;p=x.g(4)&0xffffffff;typ=x.g(5)&0xffffffff;size=x.g(6)&0xffffffff;v=x.mem.rd(p,size); meaningful=v[:12]+v[16:24] if size==24 else v[:13] if size==16 else v;trace.append(('add',q,typ,size,meaningful.hex()));x.setg(3,1)
  def out(x):x.setg(3,(x.g(3)+12)&0xffffffff)
  def cb(x):trace.append(('show',x.g(3)&0xffffffff))
  def hs(x):x.setg(3,hv)
  c.hooks[0x82435188]=add;c.hooks[0x82815a20]=hs;c.hooks[callback]=cb;c.hooks[F.address(b,'?AddEvent@?$VariableEventQueue@$0BAAAA@$0BA@@CgsModule@@')]=add
  for a,n in b.unresolved.items():
   if 'GetOutputEventQueue@StateInterface' in n:c.hooks[a]=out
   elif 'CgsIDCompress' in n:c.hooks[a]=hs
   elif 'KAC_CONNECTING_STRING_ID' in n:c.mem.wr(a,b'OnConnecting\0')
  c.setg(3,base);c.setg(4,aux);c.run(b.entry if native else 0x824cac70);outs.append((c.mem.rd(base,0x3800),trace))
 assert outs[0]==outs[1],(i,question,sign,outs[0][1],outs[1][1],[(hex(k),a,z) for k,(a,z) in enumerate(zip(outs[0][0],outs[1][0])) if a!=z][:10])
report={'passed':True,'cases':32,'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'Actual HandleShowLoginQuestion vs ARTIST824CAC70: seven question dispatches, both sign-in types, randomized full object; ID hash, member callback and queue controlled. Wrapper fields/payload checked; unspecified padding ignored. No full queue/runtime claim.'};(D/'hud-hide-comparison.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
