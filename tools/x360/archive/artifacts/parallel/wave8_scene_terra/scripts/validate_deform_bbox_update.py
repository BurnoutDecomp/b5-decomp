from pathlib import Path
import sys,json,random,struct,hashlib
sys.path.insert(0,r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\scratch\x360_sdk');import native_atomic_fixture as F
p=Path('D:/Reverse/BP-X360-Experimental-Artifacts/parallel/wave8_scene_terra/deformation-next');obj=p/'12.obj';n='?UpdateDeformedBBox@DeformableObject@';o=F.Coff(obj,n);b=F.Bundle([o]);b.select(n);A=0x40000000;SPEC=A+0x9000;VEH=A+0xb000;SP=A+0xd000;rng=random.Random(0x825e0d20);bad=[]
def run(comp,spheres,lo,hi,owner,flag):
 c=F.cpu(b);c.mem.add(A,0x10000);c.mem.add(0x82fb9000,0x1000);tol=struct.pack('>4f',*[.01]*4);c.mem.wr(0x82fb9b30,tol);c.mem.wr(F.address(b,'KVF_DEFORMED_BBOX_TOLERANCE'),tol);c.mem.wr(A+0x18e0,SPEC.to_bytes(4,'big'));c.mem.wr(A+0x194c,VEH.to_bytes(4,'big'));c.mem.wr(SPEC+0x652,bytes([len(spheres)]));c.mem.wr(A+0x66f0,lo);c.mem.wr(A+0x6700,hi);c.mem.wr(A+0x6710,bytes([owner])+bytes(7));c.mem.wr(VEH+0x1436,bytes([flag]));
 for i,v in enumerate(spheres):
  c.mem.wr(A+0x1950+i*0x1b0+0x19c,(SP+i*16).to_bytes(4,'big'));c.mem.wr(SP+i*16,v)
 c.setg(3,A);c.run(b.entry if comp else 0x825e0d20);return c.mem.rd(VEH+0x6d0,32).hex(),c.mem.rd(VEH+0x1436,1).hex()
for i in range(128):
 rnd=lambda n:struct.pack('>'+str(n)+'f',*[rng.uniform(-5,5) for _ in range(n)])
 spheres=[rnd(4) for j in range(i%5)];lo=rnd(4);hi=rnd(4)
 for owner in (1,2):
  x=run(False,spheres,lo,hi,owner,0xaa);y=run(True,spheres,lo,hi,owner,0xaa)
  if x!=y:bad.append({'case':i,'owner':owner,'original':x,'compiled':y});break
 if bad:break
print('mismatches',len(bad),bad[:1]);(p/'bbox-update-comparison.json').write_text(json.dumps({'cases':256,'mismatches':bad,'scope':'whole original UpdateDeformedBBox vs emitted object, 0-4 finite arbitrary sphere xyzW/radii, both owner branches and poisoned flags, compares full bounds xyzw and crash flag; runtime tolerance initialized, no nonfinite/hardware claim','object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest()},indent=2)+'\n');assert not bad
