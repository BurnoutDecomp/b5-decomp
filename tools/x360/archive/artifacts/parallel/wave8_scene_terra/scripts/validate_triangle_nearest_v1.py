import sys,random,json,struct,hashlib
from pathlib import Path
sys.path.insert(0,r"C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\scratch\x360_sdk");import native_atomic_fixture as F
out=Path(r"D:\Reverse\BP-X360-Experimental-Artifacts\parallel\wave8_scene_terra\volume-baseline");obj=out/"GPTriangle-nearest-v1.obj";b=F.Bundle([F.Coff(obj,"?TriangleNearestPointRegion@")]);b.select("?TriangleNearestPointRegion@");rng=random.Random(0x82bba748);bad=[];O=0x50000000;S=0x50001000;T=0x50002000;regions={}
for i in range(256):
 rows=[[rng.uniform(-5,5) for _ in range(4)] for k in range(4)]
 if i%8==0:rows[3]=rows[2][:]
 values=[struct.unpack(">4I",struct.pack(">4f",*row)) for row in rows];initial=[rng.randbytes(0x40) for _ in range(3)];result=[]
 for compiled in (False,True):
  c=F.cpu(b)
  for addr,data in zip([O,S,T],initial):c.mem.add(addr,0x40,data)
  c.setg(3,O);c.setg(4,S);c.setg(5,T)
  for k,v in enumerate(values):c.vr[k+1]=v
  c.run(b.entry if compiled else 0x82bba748);result.append((c.g(3)&0xffffffff,*[c.mem.rd(addr,0x40) for addr in [O,S,T]]))
 regions[str(result[0][0])]=regions.get(str(result[0][0]),0)+1
 if result[0]!=result[1]:bad.append({"case":i,"original_region":result[0][0],"compiled_region":result[1][0],"original":[x.hex() for x in result[0][1:]],"compiled":[x.hex() for x in result[1][1:]]})
print("cases",256,"regions",regions,"mismatches",len(bad),bad[:1]);(out/"triangle-nearest-v1-comparison.json").write_text(json.dumps({"cases":256,"regions":regions,"mismatches":bad,"object_sha256":hashlib.sha256(obj.read_bytes()).hexdigest(),"scope":"actual whole GPTriangle.cpp 82BBA748 nearest provider, real v1–v4, finite arbitrary xyzw and degenerate triangles, complete output guards; emulator model"},indent=2))
