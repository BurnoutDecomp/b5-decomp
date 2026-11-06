from pathlib import Path
import json,random,struct,sys,hashlib
P=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(P/'scratch/x360_sdk'));from native_atomic_fixture import Coff,Bundle,AtomicCPU;import validate_native_math as M
O=Path('D:/Reverse/BP-X360-Experimental-Artifacts/ice-geometry-a946-terra');j=json.loads((O/'geometry-build-v1/compile-results.json').read_text());row=next(x for x in j['results'] if x['source'].endswith('ICEControllerMenus.cpp'));assert row['passed'];obj=Path(row['object']);b=Bundle([Coff(obj,'?ConstructMenus@ICEController@')]);b.select('?ConstructMenus@ICEController@');rng=random.Random(0x82532170)

def run(compiled,image):
 c=b.cpu();c.__class__=AtomicCPU;c.mem.add(0x84800000,0x4000);c.mem.wr(M.S.SELF,image);c.setg(3,M.S.SELF);allocations=[];bars=[]
 def allocate(c):allocations.append(c.g(4)&0xffffffff);c.setg(3,M.S.AUX+0x1000+0x100*(len(allocations)-1))
 def menu(c):c.mem.wr((c.g(3)&0xffffffff)+0x34,struct.pack('>ff',12.,20.))
 def width(c):c.setf(1,15.)
 def bar(c):bars.append(((c.g(3)&0xffffffff)-M.S.SELF,M.bits(c.f(1)),M.bits(c.f(2)),M.bits(c.f(3)),c.g(7)&255))
 for address,hook in [(0x8252cc80,allocate),(0x82530f68,menu),(0x8252d050,width),(0x82531078,bar)]:c.hooks[address]=hook
 if compiled:
  for address,name in b.unresolved.items():
   mapping=[('?GetMemory@ICEMemory@',allocate),('?Construct@ICEWidgetMenu@',menu),('?GetWidth@ICEWidgetMenu@',width),('?Construct@ICEWidgetTimeBar@',bar)]
   selected=[f for prefix,f in mapping if name.startswith(prefix)]
   if selected:c.hooks[address]=selected[0]
   elif name.startswith('__savefpr_'):
    target=0x82c0a5b4+4*(int(name.rsplit('_',1)[1])-23);c.hooks[address]=lambda cc,t=target:cc.run(t)
   elif name.startswith('__restfpr_'):
    target=0x82c0a600+4*(int(name.rsplit('_',1)[1])-23);c.hooks[address]=lambda cc,t=target:cc.run(t)
   else:raise AssertionError(('unresolved',name))
 c.run(b.entry if compiled else 0x82531f40)
 return allocations,bars,c.mem.rd(M.S.SELF+0x7144,0x24),c.mem.rd(M.S.SELF+0x71f0,0x10)
for i in range(64):
 image=bytearray(rng.randbytes(0x9000));struct.pack_into('>I',image,0xf78,M.S.AUX);a=run(False,image);z=run(True,image)
 if a!=z:raise AssertionError((i,a,z))
r={'cases':64,'scope':'Original 82531F40 vs actual whole ICEController::ConstructMenus, compare allocator sequence, both timebar argument/register sets, nine picker floats and four rectangle floats on poisoned state. Allocation/menu/width/timebar are controlled boundaries; original image origin values 32/418 only; later global mutation not tested; other menu state and hardware unproved.','object':str(obj),'sha256':hashlib.sha256(obj.read_bytes()).hexdigest()};(O/'geometry-menus-comparison-v1.json').write_text(json.dumps(r,indent=2)+'\n');print(json.dumps(r,indent=2))
