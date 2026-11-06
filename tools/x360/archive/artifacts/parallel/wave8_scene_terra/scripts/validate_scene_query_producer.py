from pathlib import Path
import sys,json,random,hashlib
sys.path.insert(0,r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\scratch\x360_sdk');import native_atomic_fixture as F
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/parallel/wave8_scene_terra/scene-query-v1');obj=out/'0.obj';prefix='?LineTestNearest@SceneQueryInterface@SceneManagerIO@CgsSceneManager@@';b=F.Bundle([F.Coff(obj,prefix)]);b.select(prefix);A=0x40000000;rng=random.Random(0x82216FD0);bad=[]
def run(comp,x,y,values,queue):
 c=F.cpu(b);c.mem.add(A,0x1000);c.mem.wr(A+4,queue.to_bytes(4,'big'));events=[];fires=[]
 def add(cpu):events.append([cpu.g(3)&0xffffffff,cpu.mem.rd(cpu.g(4)&0xffffffff,49).hex()]);cpu.setg(3,1)
 def fire(cpu):fires.append(1)
 for n,a in [('AddEvent',0x82210718),('BeginAssert',0x82817548),('FireAssert',0x82820810),('EndAssert',0x82817558)]:
  fun=add if n=='AddEvent' else fire if n=='FireAssert' else lambda cpu:None;c.hooks[a]=fun
  for addr,sym in b.unresolved.items():
   if n in sym:c.hooks[addr]=fun
 c.hooks[F.address(b,'?AddEvent@?$BaseEventQueue@UInEventLineTestNearest')]=add
 c.setg(3,A)
 for i,v in enumerate(values):c.setg(4+i,v)
 c.vr[1]=x;c.vr[2]=y;c.run(b.entry if comp else 0x82216FD0);return events,len(fires)
for i in range(256):
 x=tuple(rng.getrandbits(32) for _ in range(4));y=tuple(rng.getrandbits(32) for _ in range(4));v=[rng.getrandbits(32),rng.getrandbits(32),rng.randrange(256),rng.getrandbits(32),[0,1,2,0xffffffff][i%4]];queue=A+0x100 if i%2 else 0;a=run(False,x,y,v,queue);z=run(True,x,y,v,queue)
 if a!=z:bad.append({'case':i,'original':a,'compiled':z})
r={'cases':256,'mismatches':bad,'scope':'whole original 82216FD0 vs actual compiled provider: arbitrary v1/v2 lane bits, r4-r8 arguments, valid/invalid exclusion and null/non-null queue with controlled non-gating assertions/AddEvent; compares queue receiver, defined event bytes 0..0x30, assertion count. Does not compare uninitialized event tail, assertion metadata, or real queue implementation','object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest()};(out/'producer-comparison.json').write_text(json.dumps(r,indent=2)+'\n');print('cases',256,'mismatches',len(bad),bad[:1]);assert not bad
