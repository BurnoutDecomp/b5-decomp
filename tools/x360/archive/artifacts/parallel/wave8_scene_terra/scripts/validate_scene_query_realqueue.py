from pathlib import Path
import sys,json,random,hashlib
sys.path.insert(0,r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\scratch\x360_sdk');import native_atomic_fixture as F
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/parallel/wave8_scene_terra/scene-query-v1');obj=out/'0.obj';prefix='?LineTestNearest@SceneQueryInterface@SceneManagerIO@CgsSceneManager@@';b=F.Bundle([F.Coff(obj,prefix)]);b.select(prefix);A=0x40000000;rng=random.Random(0x82210718);bad=[]
def run(comp,x,y,values,length):
 c=F.cpu(b);c.mem.add(A,0x4000);c.mem.wr(A+4,(A+0x100).to_bytes(4,'big'));c.mem.wr(A+0x100,(A+0x1000).to_bytes(4,'big')+(3).to_bytes(4,'big')+length.to_bytes(4,'big'));c.mem.wr(A+0x1000,b'\xA5'*512);fires=[]
 def fire(cpu):fires.append(1)
 for n,a in [('BeginAssert',0x82817548),('FireAssert',0x82820810),('EndAssert',0x82817558)]:
  fun=fire if n=='FireAssert' else lambda cpu:None;c.hooks[a]=fun
  for addr,sym in b.unresolved.items():
   if n in sym:c.hooks[addr]=fun
 c.setg(3,A)
 for i,v in enumerate(values):c.setg(4+i,v)
 c.vr[1]=x;c.vr[2]=y;c.run(b.entry if comp else 0x82216FD0)
 return c.mem.rd(A+0x108,4).hex(),c.mem.rd(A+0x1000+length*64,49).hex(),len(fires)
for i in range(128):
 x=tuple(rng.getrandbits(32) for _ in range(4));y=tuple(rng.getrandbits(32) for _ in range(4));v=[rng.getrandbits(32),rng.getrandbits(32),rng.randrange(256),rng.getrandbits(32),i%2];length=i%3;a=run(False,x,y,v,length);z=run(True,x,y,v,length)
 if a!=z:bad.append({'case':i,'original':a,'compiled':z})
r={'cases':128,'mismatches':bad,'scope':'actual whole original 82216FD0 -> real AddEvent 82210718 versus actual compiled provider -> its emitted real AddEvent; compares queue length, defined 49 event bytes and assertion count for arbitrary vector bits/scalars, valid modes and queue lengths 0..2 below capacity; assert services controlled; event padding, assert metadata, concurrent threads untested','object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest()};(out/'producer-realqueue-comparison.json').write_text(json.dumps(r,indent=2)+'\n');print('cases',128,'mismatches',len(bad),bad[:1]);assert not bad
