from pathlib import Path
import sys,json,random,struct,hashlib
sys.path.insert(0,r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\scratch\x360_sdk');import native_atomic_fixture as F
p=Path('D:/Reverse/BP-X360-Experimental-Artifacts/parallel/wave8_scene_terra/deformation-next');obj=p/'12.obj';n='?IsUsingSweptSpheres@DeformableObject@';o=F.Coff(obj,n);b=F.Bundle([o]);b.select(n);A=0x40000000;VEH=A+0x8000;rng=random.Random(0x825b42a8);bad=[]
def run(comp,speed,enabled,crashed):
 c=F.cpu(b);c.mem.add(A,0x10000);c.mem.add(0x83017000,0x1000);scale=struct.pack('>4f',*[.447039992]*4);c.mem.wr(0x83017fe0,scale);c.mem.wr(F.address(b,'KVF_SWEPT_SPHERE_SPEED_SCALE'),scale);c.mem.wr(A+0x194c,VEH.to_bytes(4,'big'));c.mem.wr(A+0x672a,bytes([enabled]));c.mem.wr(VEH+0x6c0,speed);c.mem.wr(VEH+0x710,bytes([crashed]));c.setg(3,A);c.run(b.entry if comp else 0x825b42a8);return c.g(3)&0xffffffff
for i in range(128):
 speed=struct.pack('>4f',*[rng.uniform(-10,40) for _ in range(4)])
 for enabled,crashed in [(0,0),(1,0),(1,1)]:
  x=run(False,speed,enabled,crashed);y=run(True,speed,enabled,crashed)
  if x!=y:bad.append({'i':i,'enabled':enabled,'crashed':crashed,'original':x,'compiled':y});break
 if bad:break
print('mismatches',len(bad),bad[:1]);(p/'bbox-swept-comparison.json').write_text(json.dumps({'cases':384,'mismatches':bad,'scope':'whole IsUsingSweptSpheres original vs emitted object, 128 finite distinct-lane speeds across disabled, enabled and crashing branches; initialized conversion constant, no nonfinite/hardware claim','object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest()},indent=2)+'\n');assert not bad
