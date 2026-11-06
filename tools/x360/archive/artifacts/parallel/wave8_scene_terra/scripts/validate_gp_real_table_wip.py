import sys,random,json,struct,hashlib
from pathlib import Path
sys.path.insert(0,r"C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\scratch\x360_sdk");import native_atomic_fixture as F
sys.path.insert(0,r"D:\Reverse\BP-Decomp_Workflow\tools\re");import x360rd
out=Path(r"D:\Reverse\BP-X360-Experimental-Artifacts\parallel\wave8_scene_terra\volume-baseline");names=["BoxVolume","GPRegistration","GPSphere","GPCapsule","GPTriangle","GPBox","GPCylinder"];prefixes=["?CreateGPInstance@BoxVolume@","?g_aGPVolumeMethods@","?GetMaximumFeature@GPSphere@","?GetMaximumFeature@GPCapsule@","?GetMaximumFeature@GPTriangle@","?GetMaximumFeature@GPBox@","?GetMaximumFeature@GPCylinder@"]
paths=[out/(n+"-table-wip.obj") for n in names];b=F.Bundle([F.Coff(p,s) for p,s in zip(paths,prefixes)]);rng=random.Random(0x82ba8100);bad=[];A=0x50000000;O=0x50001000;T=0x50002000;targets={"SphereVolume":[0x82ba8088,0x82ba80a8,0x82ba80c0,0x82ad5078],"BoxVolume":[0x82ba8918,0x82ba8650,0x82ba9238,0x82ad5078]}
for typ,addr,gp in [("SphereVolume",0x82ba8100,"GPSphere"),("BoxVolume",0x82ba92e8,"GPBox")]:
 b.select("?CreateGPInstance@"+typ+"@");expected=[F.address(b,"?"+slot+"@"+gp+"@") for slot in ["GetMaximumFeature","GetInterval","GetIntervals","GetBBox"]]
 for i in range(64):
  src=bytearray(rng.randbytes(0x100));tm=bytearray(rng.randbytes(0x100));initial=rng.randbytes(0x200)
  for j in list(range(0,0x40,4))+[0x44,0x48,0x4c,0x50]:struct.pack_into(">f",src,j,rng.uniform(-100,100))
  for j in range(0,0x40,4):struct.pack_into(">f",tm,j,rng.uniform(-5,5))
  result=[]
  for compiled in (False,True):
   c=F.cpu(b);c.mem.add(A,0x100,src);c.mem.add(O,0x200,initial);c.mem.add(T,0x100,tm)
   if not compiled:c.mem.add(0x82f91800,0x200,x360rd.rd(0x82f91800,0x200));c.mem.add(0x82cda000,0x1000,x360rd.rd(0x82cda000,0x1000))
   c.setg(3,A);c.setg(4,O);c.setg(5,T if i&1 else 0);c.run(b.entry if compiled else addr);data=bytearray(c.mem.rd(O,0x200));method=list(struct.unpack_from(">4I",data,0xa4));struct.pack_into(">4I",data,0xa4,0,0,0,0);result.append((data,c.mem.rd(A,0x100),c.mem.rd(T,0x100),c.g(3)&0xffffffff,method))
  if result[0][:4]!=result[1][:4] or result[0][4]!=targets[typ] or result[1][4]!=expected:bad.append({"type":typ,"case":i,"original":result[0][4],"compiled":result[1][4],"expected":expected})
print("cases",128,"mismatches",len(bad),bad[:2]);report={"cases":128,"mismatches":bad,"objects":{p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in paths},"scope":"actual BoxVolume CreateGPInstance and actual linked member table, sphere and box paths, null/non-null transforms; method pointers compared to genuine original and actual symbol identities, remaining GP/full guards exact. No method dispatch or full consumer claim."};(out/"gp-real-table-producer-wip-comparison.json").write_text(json.dumps(report,indent=2));assert not bad
