import sys,random,json,struct,hashlib
from pathlib import Path
sys.path.insert(0,r"C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\scratch\x360_sdk");import native_atomic_fixture as F
out=Path(r"D:\Reverse\BP-X360-Experimental-Artifacts\parallel\wave8_scene_terra\volume-baseline");obj=out/"PrimitiveIntersect-nan-wip.obj";b=F.Bundle([F.Coff(obj,"?GPTriangleAcceptContactNormal@")]);b.select("?GPTriangleAcceptContactNormal@");rng=random.Random(0x82baa600);bad=[];A=0x50000000
for i in range(256):
 src=bytearray(rng.randbytes(0x100))
 for j in range(0,0x80,4):struct.pack_into(">f",src,j,rng.uniform(-2,2))
 for j in range(0x98,0xa4,4):struct.pack_into(">f",src,j,rng.uniform(-1,1))
 vec=struct.unpack(">4I",struct.pack(">4f",*(rng.uniform(-2,2) for _ in range(4))));result=[]
 for compiled in (False,True):
  c=F.cpu(b);c.mem.add(A,0x100,src);c.setg(3,A);c.vr[1]=vec;c.run(b.entry if compiled else 0x82baa600);result.append((c.g(3)&0xffffffff,c.mem.rd(A,0x100)))
 if result[0]!=result[1]:bad.append({"case":i,"original":result[0][0],"compiled":result[1][0]})
print("cases",256,"mismatches",len(bad),bad[:8]);report={"cases":256,"mismatches":bad,"object_sha256":hashlib.sha256(obj.read_bytes()).hexdigest(),"scope":"whole PrimitiveIntersect object vs ARTIST 82BAA600; arbitrary finite row/normal, flag bits and complete input guards; no whole TU runtime claim"};(out/"triangle-accept-native-nan-v5-comparison.json").write_text(json.dumps(report,indent=2))
