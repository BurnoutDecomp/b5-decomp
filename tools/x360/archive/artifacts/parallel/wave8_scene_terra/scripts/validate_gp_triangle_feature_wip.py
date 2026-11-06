import sys,random,json,struct,hashlib
from pathlib import Path
sys.path.insert(0,r"C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\scratch\x360_sdk")
import native_atomic_fixture as F
out=Path(r"D:\Reverse\BP-X360-Experimental-Artifacts\parallel\wave8_scene_terra\volume-baseline")
objs=[out/"GPTriangle-native-wip.obj",out/"Feature-native-wip.obj"]
b=F.Bundle([F.Coff(objs[0],"?GetMaximumFeature@GPTriangle@"),F.Coff(objs[1],"?BuildEdgePlanes@Feature@")]);b.select("?GetMaximumFeature@GPTriangle@")
rng=random.Random(0x82bba158);A=0x50000000;O=0x50001000;bad=[];branches={}
for i in range(192):
 src=bytearray(rng.randbytes(0x100)); initial=rng.randbytes(0x300)
 for j in range(0,0x80,4):struct.pack_into(">f",src,j,rng.uniform(-3,3))
 # Alternate near-aligned and ordinary finite directions to exercise both arms.
 vec=[rng.uniform(-2,2) for _ in range(4)]
 if i%3==0:
  vals=struct.unpack_from(">4f",src,0x10);vec=[x*5 for x in vals]
 elif i%3==1:
  rows=[(0,0,0,0),(0,0,1,0),(1,0,0,0),(0,1,0,0),(1,0,0,0),(0,-1,0,0),(-0.70710677,0.70710677,0,0),(1,1,1,0)]
  for row,vals in enumerate(rows):struct.pack_into(">4f",src,row*16,*vals)
  vec=[(i%7-3)*0.01, 1.0, 0.0, rng.uniform(-2,2)]
 bits=struct.unpack(">4I",struct.pack(">4f",*vec));results=[]
 for compiled in (False,True):
  c=F.cpu(b);c.mem.wr(0x44000000,bytes([i&255])*0x10000);c.mem.add(A,0x100,src);c.mem.add(O,0x300,initial)
  def copy(cpu):
   dst=cpu.g(3)&0xffffffff;src=cpu.g(4)&0xffffffff;n=cpu.g(5)&0xffffffff;cpu.mem.wr(dst,cpu.mem.rd(src,n));cpu.setg(3,dst)
  c.hooks[0x82c0a630]=copy
  for addr,name in b.unresolved.items():
   if name=="memcpy":c.hooks[addr]=copy
  if compiled: eps=F.address(b,"VEC_EPSILON")
  else:c.mem.add(0x8327efd0,0x10);eps=0x8327efd0
  c.mem.wr(eps,bytes.fromhex("34000000")*4);c.setg(3,A);c.setg(4,i&1);c.setg(5,O);c.vr[1]=bits
  c.run(b.entry if compiled else 0x82bba158);results.append((c.mem.rd(O,0x300),c.mem.rd(A,0x100)))
 count=struct.unpack_from(">i",results[0][0],0x230)[0];branches[str(count)]=branches.get(str(count),0)+1
 if results[0]!=results[1]:bad.append({"case":i,"edges":count,"original":results[0][0].hex(),"compiled":results[1][0].hex()})
print("cases",192,"branches",branches,"mismatches",len(bad),bad[:1]);(out/"gp-triangle-feature-wip-comparison.json").write_text(json.dumps({"cases":192,"branch_counts":branches,"mismatches":bad,"object_sha256":{p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in objs},"scope":"actual whole GPTriangle and Feature objects composed against ARTIST 82BBA158 and 82BB9D68, initialized epsilon, full guards, finite inputs; emulator reciprocal-estimate limits, no GP dispatch claim"},indent=2));assert not bad
