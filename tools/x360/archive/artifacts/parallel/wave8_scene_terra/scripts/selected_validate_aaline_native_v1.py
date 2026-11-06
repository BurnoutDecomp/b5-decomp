import sys,random,json,struct,hashlib
from pathlib import Path
sys.path.insert(0,r"C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\scratch\x360_sdk");import native_atomic_fixture as F
out=Path(r"D:\Reverse\BP-X360-Experimental-Artifacts\parallel\wave8_scene_terra\volume-baseline");obj=out/"closure-selected-v1/AALineClipper.obj";b=F.Bundle([F.Coff(obj,"?Init@AALineClipper@")]);rng=random.Random(0x828aed60);bad=[];coverage={}
for name,original in [("?Init@AALineClipper@",0x828aed60),("??0AALineClipper@",0x82bae3c8)]:
 b.select(name);coverage[name]=0
 for i in range(128):
  initial=rng.randbytes(0x100);box=bytearray(rng.randbytes(0x40));rows=[[rng.uniform(-50,50) for _ in range(4)] for _ in range(3)]
  for j in range(0,0x20,4):struct.pack_into(">f",box,j,rng.uniform(-50,50))
  bits=[struct.unpack(">4I",struct.pack(">4f",*x)) for x in rows];results=[]
  for compiled in (False,True):
   c=F.cpu(b);c.mem.add(0x50000000,0x100,initial);c.mem.add(0x50001000,0x40,box);c.setg(3,0x50000000);c.setg(4,0x50001000)
   for k,v in enumerate(bits):c.vr[k+1]=v
   c.run(b.entry if compiled else original);results.append((c.mem.rd(0x50000000,0x100),c.mem.rd(0x50001000,0x40)))
  coverage[name]+=1
  if results[0]!=results[1]:bad.append({"function":name,"case":i,"original":results[0][0].hex(),"compiled":results[1][0].hex()})
 print(name,"mismatches",sum(x["function"]==name for x in bad),flush=True)
print("cases",coverage,"bad",len(bad),bad[:1]);(out/"aaline-native-v1-final-selected-comparison.json").write_text(json.dumps({"cases":coverage,"mismatches":bad,"object_sha256":hashlib.sha256(obj.read_bytes()).hexdigest(),"scope":"actual whole AALineClipper Init/ctor vs original, v1-v3, real AABBox r4, finite full XYZW and complete guards, emulator estimate model"},indent=2))
