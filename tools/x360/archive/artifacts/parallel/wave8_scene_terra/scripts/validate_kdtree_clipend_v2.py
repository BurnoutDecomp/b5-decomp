import sys,random,json,struct,hashlib
from pathlib import Path
sys.path.insert(0,r"C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\scratch\x360_sdk");import native_atomic_fixture as F
out=Path(r"D:\Reverse\BP-X360-Experimental-Artifacts\parallel\wave8_scene_terra\volume-baseline");obj=out/"KdTreeLineQuery-kd-native-v2.obj";b=F.Bundle([F.Coff(obj,"?ClipEnd@KdTreeLineQuery@")]);b.select("?ClipEnd@KdTreeLineQuery@");rng=random.Random(0x82bb0408);bad=[]
for i in range(128):
 initial=bytearray(rng.randbytes(0x300));n=i%33;struct.pack_into(">I",initial,0x250,n)
 for j in range(n):
  struct.pack_into(">f",initial,0x58+16*j,rng.uniform(-4,4));struct.pack_into(">f",initial,0x5c+16*j,rng.uniform(-4,4))
  if i%7==0 and j==n//2:struct.pack_into(">I",initial,0x58+16*j,0x7fc12345)
 far=rng.uniform(-3,3);results=[]
 for compiled in (False,True):
  c=F.cpu(b);c.mem.add(0x50000000,0x300,initial);c.setg(3,0x50000000);c.setf(1,far);c.run(b.entry if compiled else 0x82bb0408);results.append(c.mem.rd(0x50000000,0x300))
 if results[0]!=results[1]:bad.append({"case":i,"orig":results[0].hex(),"compiled":results[1].hex()})
print("cases",128,"mismatches",len(bad),bad[:1]);(out/"kdtree-clipend-v2-comparison.json").write_text(json.dumps({"cases":128,"mismatches":bad,"object_sha256":hashlib.sha256(obj.read_bytes()).hexdigest(),"scope":"actual KdTreeLineQuery ClipEnd vs ARTIST 82BB0408, 0..32 entries, overlap compaction, finite far and sampled NaN keys, complete guards; emulator model"},indent=2))
