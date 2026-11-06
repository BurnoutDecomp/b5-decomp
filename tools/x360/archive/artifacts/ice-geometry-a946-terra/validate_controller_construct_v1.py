from pathlib import Path
import json,random,struct,sys,hashlib
P=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(P/'scratch/x360_sdk'));from native_atomic_fixture import Coff,Bundle,AtomicCPU;import validate_native_math as M
O=Path('D:/Reverse/BP-X360-Experimental-Artifacts/ice-geometry-a946-terra');j=json.loads((O/'geometry-build-v1/compile-results.json').read_text());obj=Path(next(x['object'] for x in j['results'] if x['source'].endswith('ICEController.cpp')));b=Bundle([Coff(obj,'?Construct@ICEController@')]);b.select('?Construct@ICEController@');rng=random.Random(0x8253da30)

def run(compiled,image):
 c=b.cpu();c.__class__=AtomicCPU;c.mem.add(0x84800000,0x4000);c.mem.wr(M.S.SELF,image);c.setg(3,M.S.SELF);pointers=[M.S.AUX+0x100*(k+1) for k in range(6)];c.mem.wr(M.S.AUX,struct.pack('>6I',*pointers));c.setg(4,M.S.AUX);trace=[]
 def author(cc):trace.append(('author',cc.g(3)&0xffffffff,cc.g(4)&0xffffffff))
 def menus(cc):trace.append(('menus',cc.g(3)&0xffffffff))
 c.hooks[0x8253cc38]=author;c.hooks[0x82531f40]=menus
 if compiled:
  for addr,name in b.unresolved.items():
   if name.startswith('?Construct@ICEAuthor@'):c.hooks[addr]=author
   elif name.startswith('?ConstructMenus@ICEController@'):c.hooks[addr]=menus
   elif name.startswith('__savefpr_'):
    t=0x82c0a5b4+4*(int(name.rsplit('_',1)[1])-23);c.hooks[addr]=lambda cc,t=t:cc.run(t)
   elif name.startswith('__restfpr_'):
    t=0x82c0a600+4*(int(name.rsplit('_',1)[1])-23);c.hooks[addr]=lambda cc,t=t:cc.run(t)
   else:raise AssertionError(name)
 c.run(b.entry if compiled else 0x8253da30);return trace,c.mem.rd(M.S.SELF,0x7200)
for i in range(64):
 image=rng.randbytes(0x9000);a=run(False,image);z=run(True,image)
 if a!=z:raise AssertionError((i,a[0],z[0],[(hex(k),x,y) for k,(x,y) in enumerate(zip(a[1],z[1])) if x!=y][:12]))
r={'cases':64,'scope':'Original 8253DA30 vs actual whole ICEController::Construct: compare raw prefix through +71FF and author/menu call order/arguments on poisoned memory. Author and menus are controlled boundaries, unresolved status/tail after +7200 excluded.','object':str(obj),'sha256':hashlib.sha256(obj.read_bytes()).hexdigest()};(O/'controller-construct-prefix-comparison-v1.json').write_text(json.dumps(r,indent=2)+'\n');print(json.dumps(r,indent=2))
