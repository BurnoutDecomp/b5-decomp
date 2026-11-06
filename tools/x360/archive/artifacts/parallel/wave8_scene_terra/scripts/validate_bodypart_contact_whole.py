from pathlib import Path
import sys,json,random,hashlib,struct
sys.path.insert(0,r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\scratch\x360_sdk');import native_atomic_fixture as F
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/parallel/wave8_scene_terra/bodypart-baseline');obj=out/'bodypart.obj';name='?AddContact@PhysicalBodyPart@Deformation@BrnPhysics@@';b=F.Bundle([F.Coff(obj,name)]);b.select(name);A=0x40000000;B=A+0x1000;C=A+0x3000;rng=random.Random(0x825E2FC8);bad=[]
def run(comp,part,contact,delta):
 c=F.cpu(b);c.mem.add(A,0x5000);c.mem.wr(A,part);c.mem.wr(A+0x1e0,B.to_bytes(4,'big'));c.mem.wr(B+0x194c,C.to_bytes(4,'big'));c.mem.wr(A+0x400,contact);calls=[];fires=[]
 def getvehicle(cpu):cpu.setg(3,C)
 def getdelta(cpu):calls.append(cpu.g(4)&0xffffffff);cpu.mem.wr(cpu.g(3)&0xffffffff,delta)
 def fire(cpu):fires.append(1)
 c.hooks[0x825B2EF8]=getdelta
 for n,a in [('BeginAssert',0x82817548),('FireAssert',0x82820810),('EndAssert',0x82817558)]:
  f=fire if n=='FireAssert' else lambda cpu:None;c.hooks[a]=f
  for addr,sym in b.unresolved.items():
   if n in sym:c.hooks[addr]=f
 for addr,sym in b.unresolved.items():
  if '?GetTransformDelta@' in sym:c.hooks[addr]=getdelta
  if '?GetVehiclePhysics@' in sym:c.hooks[addr]=getvehicle
 c.setg(3,A);c.setg(4,A+0x400);c.run(b.entry if comp else 0x825E2FC8);return calls,len(fires),c.mem.rd(A,0x200).hex()
for i in range(128):
 def rnd(n):return struct.pack('>'+str(n)+'f',*[rng.uniform(-8,8) for j in range(n)])
 part=bytearray(rng.randbytes(0x200));part[0x30:0x40]=rnd(4);part[0x1b0:0x1c0]=rnd(4);part[0x1c0:0x1d0]=rnd(4);contact=bytearray(rng.randbytes(80));contact[:48]=rnd(12);contact[0x30]=[6,7,5][i%3];delta=rnd(16);x=run(False,bytes(part),bytes(contact),delta);y=run(True,bytes(part),bytes(contact),delta)
 if x!=y:bad.append({'case':i,'original':x,'compiled':y})
r={'cases':128,'mismatches':bad,'scope':'whole original AddContact825E2FC8 vs exact bounded-source compiled method with real headers; controlled GetVehiclePhysics/GetTransformDelta and assertions; finite random xyzw/contact/delta, mixed owner validity, compares transform receiver, assertion count and all0x200 object bytes incl both preserved W lanes. No whole owning TU, real vehicle service or hardware claim','object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest()};(out/'contact-whole-comparison.json').write_text(json.dumps(r,indent=2)+'\n');print('cases',128,'mismatches',len(bad),bad[:1]);assert not bad
