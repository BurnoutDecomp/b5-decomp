import sys,random,json,struct,hashlib
from pathlib import Path
sys.path.insert(0,r"C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\scratch\x360_sdk");import native_atomic_fixture as F
out=Path(r"D:\Reverse\BP-X360-Experimental-Artifacts\parallel\wave8_scene_terra\volume-baseline");paths=[out/"KdTreeLineQuery-kd-native-v2.obj",out/"AALineClipper-kd-native-v1.obj"];b=F.Bundle([F.Coff(paths[0],"??0KdTreeLineQuery@"),F.Coff(paths[1],"?Init@AALineClipper@")]);b.select("??0KdTreeLineQuery@");rng=random.Random(0x828aee80);bad=[];branches={}
for i in range(256):
 initial=rng.randbytes(0x300);tree=bytearray(rng.randbytes(0x60));struct.pack_into(">I",tree,4,i%2);struct.pack_into(">I",tree,8,i+1)
 for k in range(4):
  lo=rng.uniform(-20,-1);hi=rng.uniform(1,20);struct.pack_into(">f",tree,0x10+4*k,lo);struct.pack_into(">f",tree,0x20+4*k,hi)
 rows=[[rng.uniform(-35,35) for _ in range(4)] for _ in range(2)];bits=[struct.unpack(">4I",struct.pack(">4f",*v)) for v in rows];result=[]
 for compiled in (False,True):
  c=F.cpu(b);c.mem.add(0x50000000,0x300,initial);c.mem.add(0x50001000,0x60,tree);c.setg(3,0x50000000);c.setg(4,0x50001000);c.vr[1]=bits[0];c.vr[2]=bits[1];c.setf(1,.1);c.run(b.entry if compiled else 0x828aee80);result.append((c.mem.rd(0x50000000,0x300),c.mem.rd(0x50001000,0x60)))
 state=struct.unpack_from(">I",result[0][0],0x250)[0];branches[str(state)]=branches.get(str(state),0)+1
 if result[0]!=result[1]:bad.append({"case":i,"original":result[0][0].hex(),"compiled":result[1][0].hex()})
print("cases",256,"branches",branches,"mismatches",len(bad),bad[:1]);(out/"kdtree-native-v2-comparison.json").write_text(json.dumps({"cases":256,"branches":branches,"mismatches":bad,"objects":{p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in paths},"scope":"actual whole KdTreeLineQuery→AALineClipper vs ARTIST 828AEE80→828AED60, finite vectors, branch/leaf trees, genuine value ABI, full object/tree guards; emulator estimate limits"},indent=2))
