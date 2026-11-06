from pathlib import Path
import sys,json,random,struct,hashlib
sys.path.insert(0,r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\scratch\x360_sdk');import native_atomic_fixture as F
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/parallel/wave8_scene_terra/sphere13-line-abi');obj=out/'0.obj';names=['?TestAxisAlignedBoxAxisAlignedBox@CgsGeometric@@','?TestLineStartEndAxisAlignedBox@CgsGeometric@@'];b=F.Bundle([F.Coff(obj,n) for n in names]);A=0x40000000;rng=random.Random(0x82812460);bad=[]
def packed(v):return struct.pack('>4f',*v)
def run(comp,which,box1,box2,v1,v2):
 b.select(names[which]);c=F.cpu(b);c.mem.add(A,0x1000);c.mem.wr(A,box1);c.mem.wr(A+0x100,box2);fires=[]
 def fire(cpu):fires.append(1)
 for n,addr in [('BeginAssert',0x82817548),('FireAssert',0x82820810),('EndAssert',0x82817558)]:
  fn=fire if n=='FireAssert' else lambda cpu:None;c.hooks[addr]=fn
  for a,s in b.unresolved.items():
   if n in s:c.hooks[a]=fn
 for a,s in b.unresolved.items():
  if s.startswith('__save') or s.startswith('__rest'):c.hooks[a]=lambda cpu:None
 c.setg(3,A);c.setg(4,A+0x100);c.vr[1]=v1;c.vr[2]=v2;c.run(b.entry if comp else (0x82812460 if which==0 else 0x82812498));return list(c.vr[1]),len(fires)
for i in range(128):
 lo=[rng.uniform(-5,0) for j in range(3)];hi=[rng.uniform(0,5) for j in range(3)];delta=[rng.uniform(-5,5) for j in range(3)];box1=packed([*lo,0])+packed([*hi,0]);box2=packed([*(lo[j]+delta[j] for j in range(3)),0])+packed([*(hi[j]+delta[j] for j in range(3)),0]);vs=[struct.unpack('>4I',packed([*[rng.uniform(-8,8) for j in range(3)],rng.uniform(-8,8)])) for k in range(2)]
 for which in (0,1):
  a=run(False,which,box1,box2,*vs);z=run(True,which,box1,box2,*vs)
  if a!=z:bad.append({'case':i,'which':which,'original':a,'compiled':z})
r={'cases_per_function':128,'mismatches':bad,'scope':'whole original 82812460 and 82812498 vs actual compiled genuine MaskScalar by-value APIs; all returned v1 mask bits and controlled assert count for finite random boxes/line xyz/W inputs, emulator reciprocal estimate model; no NaN, exceptional thresholds, full collision or runtime claim','object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest()};(out/'mask-comparison.json').write_text(json.dumps(r,indent=2)+'\n');print('mismatches',len(bad),bad[:1]);assert not bad
