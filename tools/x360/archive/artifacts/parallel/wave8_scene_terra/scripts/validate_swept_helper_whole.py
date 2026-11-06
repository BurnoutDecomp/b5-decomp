from pathlib import Path
import sys,json,random,struct,hashlib
sys.path.insert(0,r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\scratch\x360_sdk');import native_atomic_fixture as F
p=Path('D:/Reverse/BP-X360-Experimental-Artifacts/parallel/wave8_scene_terra/deformation-next');obj=p/'triangle.obj';n='?Intersect2DCircleWithTriangleSOA@';o=F.Coff(obj,n);b=F.Bundle([o]);b.select(n);A=0x40000000;rng=random.Random(0x82839ac0);bad=[]
def run(comp,vecs):
 c=F.cpu(b);c.mem.add(A,0x1000);c.mem.wr(A,b'\xa5'*96)
 for i in range(13):c.vr[i+1]=struct.unpack('>4I',vecs[i])
 for i in range(5):c.mem.wr((c.g(1)+0xe0+16*i)&0xffffffff,vecs[i+13])
 c.setg(3,A+16);c.setg(4,A+48);c.run(b.entry if comp else 0x82839ac0);return tuple(c.vr[1]),c.mem.rd(A,96).hex()
for i in range(128):
 rnd=lambda n:struct.pack('>'+str(n)+'f',*[rng.uniform(-4,4) for _ in range(n)])
 vecs=[rnd(4) for j in range(18)];x=run(False,vecs);y=run(True,vecs)
 if x!=y:bad.append({'case':i,'original':x,'compiled':y});break
print('mismatches',len(bad),bad[:1]);(p/'swept-helper-whole-comparison.json').write_text(json.dumps({'cases':128,'mismatches':bad,'scope':'whole Intersect2DCircleWithTriangleSOA 82839AC0 vs actual XDK helper, 18 distinct finite raw-vector args including five stack args, compares return v1 and two full output rows plus guards; no hardware/nonfinite claim','object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest()},indent=2)+'\n');assert not bad
