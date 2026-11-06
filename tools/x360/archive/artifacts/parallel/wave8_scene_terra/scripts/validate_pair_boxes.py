from pathlib import Path
import sys,json,random,hashlib,struct
sys.path.insert(0,r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\scratch\x360_sdk');import native_atomic_fixture as F
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/parallel/wave8_scene_terra/sphere13-pair');obj=out/'pair.obj';name='?AddPrimitivePair@PrimitivePairListBuilder@CgsCollision@CgsSceneManager@@QAAXPAUBox@CgsGeometric@@0MGG@Z';b=F.Bundle([F.Coff(obj,name)]);b.select(name);header=F.address(b,'?AddCollisionHeader@PrimitivePairListBuilder@CgsCollision@CgsSceneManager@@AAAXW4EVolumeType@PrimitivePairList@23@0');alloc=F.address(b,'?AllocateMemory@PrimitivePairListBuilder@CgsCollision@CgsSceneManager@@');A=0x40000000;rng=random.Random(0x82814708);bad=[]
def box(p,d):return b''.join(struct.pack('>4f',*x) for x in [(1.,0.,0.,0.),(0.,1.,0.,0.),(0.,0.,1.,0.),(*p,0.),(*d,0.)])
def run(comp,x,y,tags):
 c=F.cpu(b);c.mem.add(A,0x2000);c.mem.wr(A,bytes(64));c.mem.wr(A+0x100,x);c.mem.wr(A+0x200,y);c.mem.wr(A+0x800,b'\xA5'*0x200);calls=[];fires=[];allocs=[]
 def h(cpu):calls.append([cpu.g(3),cpu.g(4),cpu.g(5),cpu.f(1),cpu.g(7),cpu.g(8)])
 def al(cpu):allocs.append(cpu.g(4));cpu.setg(3,A+0x800+0x100*(len(allocs)-1))
 def fire(cpu):fires.append(1)
 c.hooks[header]=h;c.hooks[0x82814480]=h;c.hooks[alloc]=al;c.hooks[0x82812098]=al
 for n,a in [('BeginAssert',0x82817548),('FireAssert',0x82820810),('EndAssert',0x82817558)]:
  fn=fire if n=='FireAssert' else lambda cpu:None;c.hooks[a]=fn
  for addr,sym in b.unresolved.items():
   if n in sym:c.hooks[addr]=fn
 c.setg(3,A);c.setg(4,A+0x100);c.setg(5,A+0x200);c.setf(1,0.25);c.setg(7,tags[0]);c.setg(8,tags[1]);c.run(b.entry if comp else 0x82814708);return calls,allocs,len(fires),c.mem.rd(A+6,2).hex(),c.mem.rd(A+0x800,80).hex(),c.mem.rd(A+0x900,80).hex()
for i in range(128):
 p=[float(rng.randrange(-4,5)) for j in range(3)];d=[float(rng.randrange(1,5)) for j in range(3)];q=p[:] if i%2 else [p[0]+float(rng.randrange(1,4)),p[1],p[2]];e=d[:];x=box(p,d);y=box(q,e);tags=[rng.randrange(65536),rng.randrange(65536)];a=run(False,x,y,tags);z=run(True,x,y,tags)
 if a!=z:bad.append({'case':i,'original':a,'compiled':z})
r={'cases':128,'mismatches':bad,'scope':'whole actual original 82814708 vs compiled AddPrimitivePair(Box,Box) with actual genuine VPU Dot/Subtract/Magnitude; axis-aligned finite integer boxes, coincident and separated; controlled header/allocator/asserts, compares header args, allocation sizes, assertion count, test count and both copied80-byte boxes. No arbitrary FP/NaN/allocator behavior/hardware claim','object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest()};(out/'pair-comparison.json').write_text(json.dumps(r,indent=2)+'\n');print('cases',128,'mismatches',len(bad),bad[:1]);assert not bad
