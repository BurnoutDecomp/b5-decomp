import sys, random, json, struct, hashlib
from pathlib import Path
sys.path.insert(0,r"C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\scratch\x360_sdk")
import native_atomic_fixture as F
out=Path(r"D:\Reverse\BP-X360-Experimental-Artifacts\parallel\wave8_scene_terra\volume-baseline")
obj=out/"Feature-native-wip.obj"
b=F.Bundle([F.Coff(obj,"?BuildEdgePlanes@Feature@")]); b.select("?BuildEdgePlanes@Feature@")
rng=random.Random(0x82bb9d68); A=0x50000000; P=0x50001000; bad=[]
for i in range(192):
 initial=bytearray(rng.randbytes(0x300)); count=i%9; struct.pack_into(">i",initial,0x230,count)
 for k in range(count):
  for lane in range(4): struct.pack_into(">f",initial,0x20+0x40*k+4*lane,rng.uniform(-5,5))
 direction=b"".join(struct.pack(">f",rng.uniform(-5,5)) for _ in range(4))+rng.randbytes(48)
 results=[]
 for compiled in (False,True):
  c=F.cpu(b); c.mem.add(A,0x300,initial);c.mem.add(P,0x40,direction)
  if compiled: addr=F.address(b,"VEC_EPSILON")
  else: c.mem.add(0x8327efd0,0x10);addr=0x8327efd0
  c.mem.wr(addr,bytes.fromhex("34000000")*4);c.setg(3,A);c.setg(4,i&1);c.setg(5,P);c.run(b.entry if compiled else 0x82bb9d68)
  results.append((c.mem.rd(A,0x300),c.mem.rd(P,0x40)))
 if results[0]!=results[1]:bad.append({"case":i,"original":results[0][0].hex(),"compiled":results[1][0].hex()})
print("cases",192,"mismatches",len(bad),bad[:1]);
(out/"feature-planes-wip-comparison.json").write_text(json.dumps({"cases":192,"mismatches":bad,"object_sha256":hashlib.sha256(obj.read_bytes()).hexdigest(),"scope":"Actual whole provider against ARTIST 82BB9D68; finite arbitrary full XYZW, counts 0..8, both winding, controlled initialized epsilon, complete object/input guards; emulator estimate limits apply."},indent=2))
assert not bad
