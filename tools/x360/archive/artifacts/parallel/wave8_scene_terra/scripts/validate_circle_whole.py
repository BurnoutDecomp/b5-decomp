from pathlib import Path
import sys,json,struct,random,hashlib
sys.path.insert(0,r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\scratch\x360_sdk');import native_atomic_fixture as F
p=Path('D:/Reverse/BP-X360-Experimental-Artifacts/parallel/wave8_scene_terra/deformation-next');obj=p/'triangle.obj';n='?Intersect2DCircleWithTriangle@';o=F.Coff(obj,n);b=F.Bundle([o]);b.select(n);A=0x40000000;rng=random.Random(0x8283daa8);bad=[]
def run(comp,vecs):
 c=F.cpu(b);c.mem.add(A,0x1000);c.mem.wr(A,b'\xa5'*48)
 for i in range(11):c.vr[i+1]=struct.unpack('>4I',vecs[i])
 c.setg(3,A+16);c.run(b.entry if comp else 0x8283daa8);return tuple(c.vr[1]),c.mem.rd(A,48).hex()
for i in range(128):
 rnd=lambda n:struct.pack('>'+str(n)+'f',*[rng.uniform(-4,4) for _ in range(n)])
 vs=[rnd(4) for j in range(11)];x=run(False,vs);y=run(True,vs)
 if x!=y:bad.append({'i':i,'original':x,'compiled':y});break
print('mismatches',len(bad),bad[:1]);(p/'circle-whole-comparison.json').write_text(json.dumps({'cases':128,'mismatches':bad,'scope':'whole Intersect2DCircleWithTriangle 8283DAA8 vs actual emitted probe, 11 distinct finite value-vector args v1-v11, full Vector2 output and guards, MaskScalar return v1; no hardware/nonfinite claim','object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest()},indent=2)+'\n');assert not bad
