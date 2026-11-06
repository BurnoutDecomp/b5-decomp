from pathlib import Path
import sys,json,random,hashlib,struct
sys.path.insert(0,r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\scratch\x360_sdk');import native_atomic_fixture as F
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/parallel/wave8_scene_terra/bodypart-bounded');obj=out/'bounded.obj';names=['?GetSphereRadius@PhysicalBodyPart@Deformation@BrnPhysics@@','?GetBoundingBox@PhysicalBodyPart@Deformation@BrnPhysics@@'];b=F.Bundle([F.Coff(obj,n) for n in names]);b.select(names[0]);A=0x40000000;rng=random.Random(0x825E7E10);bad=[]
def run(comp,body,bbox,dims):
 c=F.cpu(b);c.mem.add(A,0x1000);c.mem.wr(A,body);c.mem.wr(A+0x120,bbox);c.mem.wr(A+0x1a0,dims);valid=[]
 def isvalid(cpu):valid.append(cpu.mem.rd(cpu.g(3)&0xffffffff,80).hex());cpu.setg(3,1)
 c.hooks[0x825BEB80]=isvalid
 for addr,name in b.unresolved.items():
  if '?IsValid@Box@CgsGeometric@@' in name:c.hooks[addr]=isvalid
  elif name.startswith('__save') or name.startswith('__rest'):c.hooks[addr]=lambda cpu:None
 c.setg(3,A);c.run(b.entry if comp else 0x825E7E10);return struct.pack('>d',c.f(1)).hex(),valid
for i in range(128):
 def rnd(n):return struct.pack('>'+str(n)+'f',*[rng.uniform(-8,8) for j in range(n)])
 body=rnd(16);bbox=rnd(16);dims=rnd(4);x=run(False,body,bbox,dims);y=run(True,body,bbox,dims)
 if x!=y:bad.append({'case':i,'original':x,'compiled':y})
r={'cases':128,'mismatches':bad,'scope':'actual original GetSphereRadius825E7E10 -> original GetBoundingBox825E7D28 -> original Box::Set825E6918 vs exact bounded source bodies -> emitted real Box::Set; Box::IsValid controlled true, compares full box80 bytes at validity boundary and f1 result for finite random xyzw, VMX reciprocal estimate emulator. Whole owning CPP not yet compiled; no real validity/exception/hardware claim','object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest()};(out/'composed-radius-comparison.json').write_text(json.dumps(r,indent=2)+'\n');print('cases',128,'mismatches',len(bad),bad[:1]);assert not bad
