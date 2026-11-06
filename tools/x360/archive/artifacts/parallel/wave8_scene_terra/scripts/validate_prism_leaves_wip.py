import sys,random,json,struct,hashlib
from pathlib import Path
sys.path.insert(0,r"C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\scratch\x360_sdk");import native_atomic_fixture as F
out=Path(r"D:\Reverse\BP-X360-Experimental-Artifacts\parallel\wave8_scene_terra\volume-baseline");paths=[out/"FeaturePrism-migrate4-wip.obj",out/"FeatureEdge-rebind-wip.obj"]
b=F.Bundle([F.Coff(paths[0],"?FindIntervalOverlap@"),F.Coff(paths[1],"?constrain_point@")]);rng=random.Random(0x82bb7828);bad=[];coverage={}
for name,original in [("?FindIntervalOverlap@",0x82bb7828),("?FindEdgePointPrism@",0x82bb77c0),("?FindFacePointPrism@",0x82bb7f18)]:
 b.select(name); coverage[name]=0
 for i in range(128):
  regions=[bytearray(rng.randbytes(0x400)) for _ in range(6)];addresses=[0x50000000+k*0x1000 for k in range(6)]
  if "Interval" in name:
   for j in range(3):
    for off in range(0,0x20,4):struct.pack_into(">f",regions[j],off,rng.uniform(-5,5))
   alias=i%3; regs=[addresses[alias],addresses[1],addresses[2]]
  else:
   # result, two output vectors, edge/face feature, point feature, direction
   for j in [3,4]:
    for off in range(0,0x230,4):struct.pack_into(">f",regions[j],off,rng.uniform(-2,2))
    struct.pack_into(">I",regions[j],0, i*7)
    struct.pack_into(">i",regions[j],0x230,1+(i%4) if (j==3 and "FacePoint" in name) else 1)
    for edge in range(8):
     size=rng.uniform(0,3);struct.pack_into(">4f",regions[j],0x40+edge*0x40,*([size]*4))
   for off in range(0,16,4):struct.pack_into(">f",regions[5],off,rng.uniform(-2,2))
   regs=addresses
  results=[]
  for compiled in (False,True):
   c=F.cpu(b)
   for a,data in zip(addresses,regions):c.mem.add(a,0x400,data)
   for n,val in enumerate(regs):c.setg(3+n,val)
   c.run(b.entry if compiled else original);results.append(([c.mem.rd(a,0x400) for a in addresses],c.g(3)&0xffffffff))
  coverage[name]+=1
  if results[0]!=results[1]:bad.append({"function":name,"case":i,"original_return":results[0][1],"compiled_return":results[1][1],"differing_regions":[k for k in range(6) if results[0][0][k]!=results[1][0][k]]})
 print(name,"cases",coverage[name],"mismatches",sum(x["function"]==name for x in bad),flush=True)
report={"cases":coverage,"mismatches":bad,"object_sha256":{p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in paths},"scope":"Actual whole FeaturePrism and FeatureEdge objects vs original leaves, finite arbitrary XYZW, interval alias modes and entire guard regions; emulator model, no full prism family claim."};(out/"featureprism-leaves-wip-comparison.json").write_text(json.dumps(report,indent=2));print(bad[:5]);assert not bad
