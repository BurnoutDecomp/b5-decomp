from pathlib import Path
import sys,json,random,struct,hashlib
sys.path.insert(0,r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\scratch\x360_sdk');import native_atomic_fixture as F
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/parallel/wave8_scene_terra/bodypart-baseline');obj=out/'bodypart.obj';names=['?CalcBoundingBox@PhysicalBodyPart@Deformation@BrnPhysics@@','?UpdateBoundingBox@PhysicalBodyPart@Deformation@BrnPhysics@@'];ext='?CalculateBoundingBoxExtents@PhysicalBodyPart@Deformation@BrnPhysics@@';co=F.Coff(obj,names[0]);b=F.Bundle([co,F.Coff(obj,names[1]),F.Coff(obj,ext)]);A=0x40000000;B=A+0x300;C=A+0x500;D=A+0x1000;E=A+0x3000;rng=random.Random(0x8260ABB0);bad=[]
def run(comp,which,part,graphics,offsets,lo,hi):
 b.select(names[which]);c=F.cpu(b);c.mem.add(A,0x5000);c.mem.add(0x82FB9000,0x1000);c.mem.wr(0x82FB9DD0,struct.pack('>4f',.05,.05,.05,0.));c.mem.wr(A,part);c.mem.wr(A+0x1dc,B.to_bytes(4,'big'));c.mem.wr(A+0x1e0,D.to_bytes(4,'big'));c.mem.wr(B+8,C.to_bytes(4,'big'));c.mem.wr(C,graphics);c.mem.wr(D+0x18e0,E.to_bytes(4,'big'));c.mem.wr(E+0x660,offsets[:16]);c.mem.wr(E+0x680,offsets[16:]);calls=[]
 for name,value,sec in co.symbols.values():
  if name=='KV_MIN_BBOX_HALF_SIZE' and (0,sec) in b.bases:c.mem.wr(b.bases[(0,sec)]+value,struct.pack('>4f',.05,.05,.05,0.))
 def calc(cpu):calls.append(cpu.g(3)&0xffffffff);cpu.mem.wr(cpu.g(4)&0xffffffff,lo);cpu.mem.wr(cpu.g(5)&0xffffffff,hi)
 c.hooks[0x825E2B80]=calc;c.hooks[F.address(b,ext)]=calc;c.setg(3,A);c.setg(4,A+0x900);c.run(b.entry if comp else (0x8260ABB0 if which==0 else 0x8260ACC8));return calls,c.mem.rd(A,0x200).hex()
for i in range(128):
 def rnd(n):return struct.pack('>'+str(n)+'f',*[rng.uniform(-8,8) for j in range(n)])
 part=bytearray(rng.randbytes(0x200));part[0x120:0x160]=rnd(16);part[0x180:0x190]=rnd(4);lo=rnd(4);hi=rnd(4);graphics=rnd(16);offsets=rnd(8)
 for which in (0,1):
  x=run(False,which,bytes(part),graphics,offsets,lo,hi);y=run(True,which,bytes(part),graphics,offsets,lo,hi)
  if x!=y:bad.append({'case':i,'which':which,'original':x,'compiled':y});break
r={'cases_per_function':128,'mismatches':bad,'scope':'actual original CalcBoundingBox8260ABB0 and UpdateBoundingBox8260ACC8 vs exact bounded source methods with real headers; CalculateBoundingBoxExtents controlled, exact runtime .05 xyz/W0, finite arbitrary vectors/matrices; compares call receiver and all0x200 part bytes incl W; no full owner TU or downstream skin/runtime claim','object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest()};(out/'bbox_update-whole-comparison.json').write_text(json.dumps(r,indent=2)+'\n');print('cases',128,'mismatches',len(bad),bad[:1]);assert not bad
