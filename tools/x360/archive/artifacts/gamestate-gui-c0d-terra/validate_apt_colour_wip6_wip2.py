from pathlib import Path
import json,random,struct,sys,hashlib
P=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(P/'scratch/x360_sdk'));import native_atomic_fixture as F
O=Path('D:/Reverse/BP-X360-Experimental-Artifacts/gamestate-gui-c0d-terra');j=json.loads((O/'wip2/compile-results.json').read_text());obj=Path(next(x['object'] for x in j['results'] if x['source'].endswith('CgsAptCallbackRender.cpp')));sym='?SetColourTransform@AptCallbackRender@CgsGui@@';b=F.Bundle([F.Coff(obj,sym)]);b.select(sym);A=F.M.S.AUX;AUX=A+0x1000;DATA=A+0x4000
rng=random.Random(0x82851cb8)
def run(compiled,initial,data):
 c=F.cpu(b);c.mem.wr(AUX,initial);c.mem.wr(DATA,data);c.setg(3,DATA);c.mem.add(0x8305a000,0x1000);c.mem.wr(0x8305a6c8,struct.pack('>I',AUX))
 if compiled:
  for at,n in b.unresolved.items():
   if 'mpAptAuxInst' in n:c.mem.wr(at,struct.pack('>I',AUX))
   else:c.hooks[at]=lambda cc,name=n:(_ for _ in ()).throw(AssertionError(name))
  c.run(b.entry)
 else:
  for a in [0x82817548,0x82820810,0x82817558]:c.hooks[a]=lambda cc:(_ for _ in ()).throw(AssertionError('original assert'))
  c.run(0x82851cb8)
 return c.mem.rd(AUX+0x420,0x80)
for i in range(128):
 init=rng.randbytes(0x500);vals=[rng.uniform(-10,10) for _ in range(4)];vals2=[rng.uniform(-10,10) for _ in range(4)];data=struct.pack('>I4fI4f',0,*vals,0,*vals2);a=run(False,init,data);z=run(True,init,data)
 if a!=z:print('case',i,'diff',[hex(k) for k,(x,y) in enumerate(zip(a,z)) if x!=y]);print(a.hex(),z.hex());raise AssertionError(i)
r={'cases':128,'scope':'Original ARTIST 82851CB8 vs actual compiled SetColourTransform full 0x80-byte handler prefix; finite randomized eight colour channels and poisoned state, valid pointers only.','object':str(obj),'sha256':hashlib.sha256(obj.read_bytes()).hexdigest()};(O/'apt-colour-comparison-wip6-wip2.json').write_text(json.dumps(r,indent=2)+'\n');print(json.dumps(r,indent=2))

