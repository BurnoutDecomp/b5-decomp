from pathlib import Path
import sys,json,random,struct,hashlib
sys.path.insert(0,r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\scratch\x360_sdk');import native_atomic_fixture as F
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/parallel/wave8_scene_terra/bodypart-baseline');obj=out/'bodypart.obj';n='?CalculateBoundingBoxExtents@PhysicalBodyPart@Deformation@BrnPhysics@@';co=F.Coff(obj,n);b=F.Bundle([co]);b.select(n);A=0x40000000;B=A+0x300;C=A+0x600;rng=random.Random(0x825E2B80);bad=[]
def run(comp,orientation,points):
 c=F.cpu(b);c.mem.add(A,0x2000);c.mem.add(0x82FB9000,0x1000);c.mem.wr(A+0x1dc,B.to_bytes(4,'big'));c.mem.wr(B+8,C.to_bytes(4,'big'));c.mem.wr(C+0x40,orientation);c.mem.wr(A+0xC00,b'\xA5'*64)
 for at,vals in [(0x82FB9B00,(100000.,100000.,100000.,0.)),(0x82FB96E0,(.1,.1,.1,0.))]:c.mem.wr(at,struct.pack('>4f',*vals))
 for name,value,sec in co.symbols.values():
  if name=='KV_MIN_BBOX_SIZE':c.mem.wr(b.bases[(0,sec)]+value,struct.pack('>4f',.1,.1,.1,0.))
 calls=[];fires=[]
 def skin(cpu):
  calls.append([cpu.g(4)&0xffffffff,cpu.g(5)&0xffffffff]);cpu.mem.wr(cpu.g(3)&0xffffffff,points[len(calls)-1]);cpu.setg(3,cpu.g(3))
 def fire(cpu):fires.append(1)
 c.hooks[0x825E2560]=skin;c.hooks[F.address(b,"?CalculateSkinnedPoint@PhysicalBodyPart@")]=skin
 for k,v in b.unresolved.items():
  if v.startswith('__save') or v.startswith('__rest'):c.hooks[k]=lambda cpu:None
  elif '?CalculateSkinnedPoint@' in v:c.hooks[k]=skin
  elif 'FireAssert' in v:c.hooks[k]=fire
  elif 'BeginAssert' in v or 'EndAssert' in v:c.hooks[k]=lambda cpu:None
 c.hooks[0x82817548]=lambda cpu:None;c.hooks[0x82820810]=fire;c.hooks[0x82817558]=lambda cpu:None
 c.setg(3,A);c.setg(4,A+0xc10);c.setg(5,A+0xc30);c.run(b.entry if comp else 0x825E2B80);return calls,len(fires),c.mem.rd(A+0xc00,64).hex()
for i in range(128):
 def rnd(n):return struct.pack('>'+str(n)+'f',*[rng.uniform(-8,8) for j in range(n)])
 orientation=rnd(16);points=[rnd(4) for j in range(10)];x=run(False,orientation,points);y=run(True,orientation,points)
 if x!=y:bad.append({'case':i,'original':x,'compiled':y})
r={'cases':128,'mismatches':bad,'scope':'actual original CalculateBoundingBoxExtents825E2B80 vs bounded exact source method with real headers; controlled CalculateSkinnedPoint returns and assertions, runtime-initialized original constants, 10 finite random points and orientation xyzw; compares ordered point receiver/addresses, all min/max outputs/guards, assertion count. Full owning CPP/skin method, invalid values/hardware not claimed','object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest()};(out/'extents-whole-comparison.json').write_text(json.dumps(r,indent=2)+'\n');print('cases',128,'mismatches',len(bad),bad[:1]);assert not bad
