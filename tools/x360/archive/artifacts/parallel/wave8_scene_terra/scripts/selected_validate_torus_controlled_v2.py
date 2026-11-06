import sys,random,json,struct,hashlib
from pathlib import Path
sys.path.insert(0,r"C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\scratch\x360_sdk");import native_atomic_fixture as F
out=Path(r"D:\Reverse\BP-X360-Experimental-Artifacts\parallel\wave8_scene_terra\volume-baseline");obj=out/"closure-selected-v1/LineSegIntersect.obj";b=F.Bundle([F.Coff(obj,"?rwcTorusLineSegIntersect@")]);b.select("?rwcTorusLineSegIntersect@");rng=random.Random(0x82badab0);bad=[]
for i in range(192):
 initial=rng.randbytes(0x40);vectors=[struct.unpack(">4I",struct.pack(">4f",*(rng.uniform(-3,3) for _ in range(4)))) for _ in range(2)];R=rng.uniform(.1,3);r=rng.uniform(.1,1);results=[]
 for compiled in (False,True):
  c=F.cpu(b);c.mem.add(0x50000000,0x40,initial);trace=[]
  def solve(cpu):
   trace.append(cpu.mem.rd(cpu.g(3)&0xffffffff,20).hex());cpu.mem.wr(cpu.g(4)&0xffffffff,struct.pack(">f",.375));cpu.setg(3,i&1)
  c.hooks[F.address(b,"?SolveQuarticRoots@") if compiled else 0x82baca30]=solve;c.setg(3,0x50000000);c.vr[1]=vectors[0];c.vr[2]=vectors[1];c.setf(1,R);c.setf(2,r);c.run(b.entry if compiled else 0x82badab0);results.append((c.g(3)&0xffffffff,c.mem.rd(0x50000000,0x40),trace))
 if results[0]!=results[1]:bad.append({"case":i,"original":results[0][2],"compiled":results[1][2],"original_return":results[0][0],"compiled_return":results[1][0]})
print("cases",192,"mismatches",len(bad),bad[:2]);(out/"torus-controlled-v2-final-selected-comparison.json").write_text(json.dumps({"cases":192,"mismatches":bad,"object_sha256":hashlib.sha256(obj.read_bytes()).hexdigest(),"scope":"actual whole torus vs 82BADAB0; real v1/v2,f1/f2; controlled quartic captures all five coefficients and writes same root; full output guards; no quartic or actual torus-root claim"},indent=2))
