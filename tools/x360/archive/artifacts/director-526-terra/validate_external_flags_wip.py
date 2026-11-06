from pathlib import Path
import json,random,hashlib,sys
P=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(P/'scratch/x360_sdk'));import native_atomic_fixture as F
O=Path('D:/Reverse/BP-X360-Experimental-Artifacts/director-526-terra');j=json.loads((O/'parameters-typed-wip4/compile-results.json').read_text());obj=next(Path(x['object']) for x in j['results'] if x['source'].endswith('BrnBehaviourGameplayExternal.cpp'));b=F.Bundle([F.Coff(obj,'?Construct@BehaviourGameplayExternal@')]);b.select('?Construct@BehaviourGameplayExternal@')
orig=json.load(open('D:/Reverse/BP-Decomp_Workflow/.ida-exports/BURNOUT_X360_ARTIST.XEX/0x82224A18.json'))['assembly'].splitlines();S=F.M.S.SELF
rng=random.Random(0x82224a18);offsets=set()
for i in range(128):
 initial=rng.randbytes(0xb70);images=[]
 for compiled in (False,True):
  c=F.cpu(b);c.mem.wr(S,initial);c.setg(3,S)
  def hook(cc,n):
   if '?Construct@Behaviour@Camera@' in n: cc.mem.wr(cc.g(3)+4,bytes(9));cc.mem.wr(cc.g(3)+16,bytes(4))
   else: pass
  if compiled:
   for at,n in b.unresolved.items():c.hooks[at]=lambda cc,name=n:hook(cc,name)
   c.run(b.entry)
  else:
   for l in orig:
    if '  bl ' not in l:continue
    p=l.split();pc=int(p[0],16);n=p[2];w=F.M.V.x360rd.u32(pc);d=w&0x03fffffc;d=d-0x04000000 if d&0x02000000 else d;c.hooks[(pc+d)&0xffffffff]=lambda cc,name=n:hook(cc,name)
   c.run(0x82224a18)
  images.append(c.mem.rd(S,0xb70))
 if images[0][0xb60:0xb62] != images[1][0xb60:0xb62]:raise AssertionError(i)
 offsets.update(k for k,(x,y) in enumerate(zip(*images)) if x!=y)
r={'cases':128,'checked_offsets':['0xb60','0xb61'],'other_differing_offsets':[hex(k) for k in sorted(offsets)],'scope':'Original versus compiled whole Construct, nested constructors and VehicleRef setters mocked; only neighboring jumping and replay rotation flag bytes asserted equal. Other differences are retained for investigation, and this is not a whole constructor parity claim.','object':str(obj),'sha256':hashlib.sha256(obj.read_bytes()).hexdigest()};(O/'external-flags-ctor-comparison-wip.json').write_text(json.dumps(r,indent=2)+'\n');print(json.dumps(r,indent=2))



