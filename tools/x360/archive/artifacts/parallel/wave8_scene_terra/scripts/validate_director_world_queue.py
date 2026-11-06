from pathlib import Path
import sys,json,random,hashlib
sys.path.insert(0,r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\scratch\x360_sdk');import native_atomic_fixture as F
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/parallel/wave8_scene_terra/scene-query-combined');n='?LineTestNearest@SceneQueryInterface@BrnDirector@@';p='?LineTestNearest@SceneQueryInterface@SceneManagerIO@CgsSceneManager@@';b=F.Bundle([F.Coff(out/'1.obj',n),F.Coff(out/'0.obj',p)]);b.select(n);post=F.address(b,'?AddPostBox@?$PostOffice@UOutEventLineTestNearestResult');A=0x40000000;rng=random.Random(0x82233048);bad=[]
def run(comp,x,y,vals,postid,length):
 c=F.cpu(b);c.mem.add(A,0x4000);c.mem.wr(A,(A+0x100).to_bytes(4,'big'));c.mem.wr(A+8,(A+0x200).to_bytes(4,'big'));c.mem.wr(A+0x104,(A+0x400).to_bytes(4,'big'));c.mem.wr(A+0x400,(A+0x1000).to_bytes(4,'big')+(3).to_bytes(4,'big')+length.to_bytes(4,'big'));c.mem.wr(A+0x1000,b'\xA5'*512);posts=[];fires=[]
 def addpost(cpu):posts.append([cpu.g(3)&0xffffffff,cpu.g(4)&0xffffffff]);cpu.setg(3,postid)
 def fire(cpu):fires.append(1)
 c.hooks[post]=addpost;c.hooks[0x8222D288]=addpost
 for n0,a in [('BeginAssert',0x82817548),('FireAssert',0x82820810),('EndAssert',0x82817558)]:
  f=fire if n0=='FireAssert' else lambda cpu:None;c.hooks[a]=f
  for addr,sym in b.unresolved.items():
   if n0 in sym:c.hooks[addr]=f
 c.setg(3,A);c.setg(4,A+0x300)
 for i,v in enumerate(vals):c.setg(5+i,v)
 c.vr[1]=x;c.vr[2]=y;c.run(b.entry if comp else 0x82233048)
 return posts,c.mem.rd(A+0x408,4).hex(),c.mem.rd(A+0x1000+64*length,49).hex(),len(fires)
for i in range(128):
 x=tuple(rng.getrandbits(32) for _ in range(4));y=tuple(rng.getrandbits(32) for _ in range(4));v=[rng.getrandbits(32),rng.randrange(256),rng.getrandbits(32),i%2];id=rng.randrange(65536);length=i%3;a=run(False,x,y,v,id,length);z=run(True,x,y,v,id,length)
 if a!=z:bad.append({'case':i,'original':a,'compiled':z})
r={'cases':128,'mismatches':bad,'scope':'actual original Director82233048 -> real World82216FD0 -> real queue82210718 vs linked compiled Director+World+emitted queue; controlled AddPostBox and assertions, arbitrary raw v1/v2/scalars and postbox ids, valid mode/below-capacity queue; compares postbox receiver/arg, queue length, 49 defined event bytes and assertion count. Does not compare padding/assert metadata/concurrency/full camera','director_object_sha256':hashlib.sha256((out/'1.obj').read_bytes()).hexdigest(),'producer_object_sha256':hashlib.sha256((out/'0.obj').read_bytes()).hexdigest()};(out/'director-world-queue-comparison.json').write_text(json.dumps(r,indent=2)+'\n');print('cases',128,'mismatches',len(bad),bad[:1]);assert not bad
