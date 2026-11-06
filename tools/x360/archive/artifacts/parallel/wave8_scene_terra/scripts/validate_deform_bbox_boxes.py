from pathlib import Path
import sys,json,random,struct,hashlib
sys.path.insert(0,r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\scratch\x360_sdk');import native_atomic_fixture as F
p=Path('D:/Reverse/BP-X360-Experimental-Artifacts/parallel/wave8_scene_terra/deformation-next');obj=p/'12.obj';A=0x40000000;VEH=A+0x8000;SPEC=A+0xa000;OUT=A+0xc000;rng=random.Random(0x825e8620);bad=[]
def run(comp,which,transform,bounds,dims,offset):
 n=['?GetBoundingBox@DeformableObject@','?GetAlignedDeformedBoundingBox@DeformableObject@'][which];o=F.Coff(obj,n);b=F.Bundle([o]);b.select(n);c=F.cpu(b);c.mem.add(A,0x10000);c.mem.add(0x82fb9000,0x1000);c.mem.wr(A+0x194c,VEH.to_bytes(4,'big'));c.mem.wr(A+0x18e0,SPEC.to_bytes(4,'big'));c.mem.wr(VEH+0x10,transform);c.mem.wr(VEH+0x6d0,bounds);c.mem.wr(SPEC+0x40,dims);c.mem.wr(SPEC+0x660,offset);c.mem.wr(0x82fb95e0,struct.pack('>4f',.1,.3,.05,0));valid=[]
 for name,val,sec in o.symbols.values():
  if name=='KVF_CAR_BBOX_SHRINK' and (0,sec) in b.bases:c.mem.wr(b.bases[(0,sec)]+val,struct.pack('>4f',.1,.3,.05,0))
 def check(cpu):valid.append(cpu.mem.rd(cpu.g(3)&0xffffffff,80).hex());cpu.setg(3,1)
 c.hooks[0x825beb80]=check
 for k,v in b.unresolved.items():
  if 'IsValid@Box' in v:c.hooks[k]=check
  elif v.startswith('__save') or v.startswith('__rest'):c.hooks[k]=lambda cpu:None
 c.setg(3,A);c.setg(4,OUT);c.run(b.entry if comp else [0x825e8620,0x825e86c8][which]);return c.mem.rd(OUT,80).hex(),valid
for i in range(128):
 rnd=lambda n:struct.pack('>'+str(n)+'f',*[rng.uniform(-5,5) for _ in range(n)])
 tr,bounds,dims,offset=rnd(16),rnd(8),rnd(4),rnd(4)
 for which in (0,1):
  x=run(False,which,tr,bounds,dims,offset);y=run(True,which,tr,bounds,dims,offset)
  if x!=y:bad.append({'i':i,'which':which,'original':x,'compiled':y});break
 if bad:break
print('mismatches',len(bad),bad[:1]);(p/'bbox-boxes-comparison.json').write_text(json.dumps({'cases_per_function':128,'mismatches':bad,'scope':'whole GetBoundingBox and GetAlignedDeformedBoundingBox vs original, real emitted Box::Set, controlled IsValid=true; finite xyzW transform/spec/bounds, compares all 80 bytes and validity receiver; no hardware/nonfinite or real IsValid proof','object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest()},indent=2)+'\n');assert not bad
