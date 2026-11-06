from pathlib import Path
import sys,json,random,struct,hashlib
sys.path.insert(0,r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\scratch\x360_sdk');import native_atomic_fixture as F
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/parallel/wave8_scene_terra/deformation-next');obj=out/'9.obj';A=0x40000000;R=A+0x1000;S=A+0x9000;P=A+0xa000;rng=random.Random(0x825b9898);bad=[]
def run(name,original,mode,init,rig,sphere,sliders):
 o=F.Coff(obj,name);b=F.Bundle([o]);b.select(name);c=F.cpu(b);c.mem.add(A,0x10000);c.mem.wr(A,init);c.mem.wr(R,rig);c.mem.wr(A+0x14,R.to_bytes(4,'big'));c.mem.wr(A+0x1c,S.to_bytes(4,'big'));c.mem.wr(S+0x19c,P.to_bytes(4,'big'));c.mem.wr(P,sphere);c.mem.wr(A+(0x2c if mode==0 else 0x70),sliders);c.mem.wr(R+0x4b10,(1).to_bytes(4,'big'));c.mem.wr(A+0x20,(0 if mode==1 else 1).to_bytes(4,'big'));c.setg(3,0);c.setg(4,A);c.run(b.entry if original else (0x825b9898 if mode==0 else 0x825b98f8));return c.mem.rd(P,16).hex(),c.mem.rd(S+0x1a4,4).hex(),c.mem.rd(R+0x6728,1).hex(),c.mem.rd(R+(0x3b10 if mode==1 else 0x4b20),16).hex()
for i in range(128):
 rnd=lambda n:struct.pack('>'+str(n)+'f',*[rng.uniform(-5,5) for _ in range(n)])
 init=rng.randbytes(0x100);rig=rng.randbytes(0x8000);sphere=rnd(4);sliders=rnd(4)
 for mode in range(3):
  name='?OnSensorPositionChange@DeformationDebugComponent@' if mode==0 else '?OnPointPositionChange@DeformationDebugComponent@';x=run(name,False,mode,init,rig,sphere,sliders);y=run(name,True,mode,init,rig,sphere,sliders)
  if x!=y:bad.append({'case':i,'mode':mode,'original':x,'compiled':y});break
 if bad:break
print('mismatches',len(bad),bad[:1]);(out/'debug-callback-comparison.json').write_text(json.dumps({'cases_per_function':128,'mismatches':bad,'scope':'whole original sensor and point callbacks vs actual whole object; 128 finite sensor, tag and driven cases each with poisoned fields, compares sphere xyzW/scratch, dirty flag and point xyzW; valid object pointers, no null/nonfinite/hardware claim','object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest()},indent=2)+'\n');assert not bad
