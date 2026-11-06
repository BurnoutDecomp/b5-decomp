from pathlib import Path
import sys,json,random,struct,hashlib
sys.path.insert(0,r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\scratch\x360_sdk');import native_atomic_fixture as F
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/parallel/wave8_scene_terra/scene-query-combined');O=Path('D:/Reverse/BP-X360-Experimental-Artifacts/camera-three-c0d-terra');j=json.loads((O/'camera-final-v1/compile-results.json').read_text());obj=Path(next(x['object'] for x in j['results'] if x['source'].endswith('BrnCollisionPolicyAttachedToVehicle.cpp')));n='?RequestFrustumLineTests@FrustrumCollisionResolver';d='?LineTestNearest@SceneQueryInterface@BrnDirector@@';p='?LineTestNearest@SceneQueryInterface@SceneManagerIO@CgsSceneManager@@';b=F.Bundle([F.Coff(obj,n),F.Coff(out/'1.obj',d),F.Coff(out/'0.obj',p)]);b.select(n);post=F.address(b,'?AddPostBox@?$PostOffice@UOutEventLineTestNearestResult');S=F.M.S.SELF;A=F.M.S.AUX;B=0x41000000;rng=random.Random(0x8223FD70);bad=[]
def run(comp,box,vec):
 c=F.cpu(b);c.mem.add(B,0x3000);c.mem.wr(S,box);c.mem.wr(A,(B).to_bytes(4,'big'));c.mem.wr(A+8,(B+0x100).to_bytes(4,'big'));c.mem.wr(B+4,(B+0x200).to_bytes(4,'big'));c.mem.wr(B+0x200,(B+0x1000).to_bytes(4,'big')+(8).to_bytes(4,'big')+(0).to_bytes(4,'big'));c.mem.wr(B+0x1000,b'\xA5'*512);posts=[];fires=[]
 for i,v in enumerate(vec):c.vr[i+1]=v
 if comp:
  for name,(oi,off,sec) in b.definitions.items():
   if name.startswith('?sTestStartWidthPadding@'):c.mem.wr(b.bases[(oi,sec)]+off,struct.pack('>4f',*[.1]*4))
   if name.startswith('?sTestLengthPadding@'):c.mem.wr(b.bases[(oi,sec)]+off,struct.pack('>4f',*[1.]*4))
 else:
  c.mem.add(0x82faa000,0x1000);c.mem.wr(0x82faab50,struct.pack('>4f',*[.1]*4));c.mem.wr(0x82faa910,struct.pack('>4f',*[1.]*4))
 def addpost(cpu):posts.append([cpu.g(3)&0xffffffff,cpu.g(4)-S]);cpu.setg(3,len(posts)-1)
 def fire(cpu):fires.append(1)
 c.hooks[post]=addpost;c.hooks[0x8222D288]=addpost
 for nm,addr in [('BeginAssert',0x82817548),('FireAssert',0x82820810),('EndAssert',0x82817558)]:
  fun=fire if nm=='FireAssert' else lambda cpu:None;c.hooks[addr]=fun
  for at,name in b.unresolved.items():
   if nm in name:c.hooks[at]=fun
 for at,name in b.unresolved.items():
  if name.startswith('__save') or name.startswith('__rest'):c.hooks[at]=lambda cpu:None
 c.setg(3,S);c.setg(4,A);c.run(b.entry if comp else 0x8223FD70)
 return posts,c.mem.rd(B+0x208,4).hex(),[c.mem.rd(B+0x1000+64*i,49).hex() for i in range(4)],c.mem.rd(S,len(box)).hex(),len(fires)
for i in range(128):
 box=rng.randbytes(0x180);vec=[struct.unpack('>4I',struct.pack('>4f',*[rng.uniform(-4,4) for _ in range(4)])) for k in range(5)];a=run(False,box,vec);z=run(True,box,vec)
 if a!=z:bad.append({'case':i,'original':a,'compiled':z});break
r={'cases':128,'mismatches':bad,'scope':'actual camera RequestFrustumLineTests8223FD70 -> Director82233048 -> World82216FD0 -> real queue82210718 versus linked compiled camera/Director/World and emitted real queue; finite random camera vector inputs, four controlled postbox registrations and assert handlers, compares registration receivers/postboxes, queue count, four 49-byte defined events and full resolver state. Emulator estimate model; event padding, allocator/concurrency/hardware/full camera excluded','camera_object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'director_object_sha256':hashlib.sha256((out/'1.obj').read_bytes()).hexdigest(),'producer_object_sha256':hashlib.sha256((out/'0.obj').read_bytes()).hexdigest()};(out/'camera-director-world-queue-comparison.json').write_text(json.dumps(r,indent=2)+'\n');print('mismatches',len(bad),bad[:1]);assert not bad
