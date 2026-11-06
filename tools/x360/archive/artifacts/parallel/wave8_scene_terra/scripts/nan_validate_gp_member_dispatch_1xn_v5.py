import sys,random,json,struct,hashlib
from pathlib import Path
sys.path.insert(0,r"C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\scratch\x360_sdk");import native_atomic_fixture as F
out=Path(r"D:\Reverse\BP-X360-Experimental-Artifacts\parallel\wave8_scene_terra\volume-baseline");obj=out/"PrimitiveIntersect-nan-wip.obj";b=F.Bundle([F.Coff(obj,"?GPInstanceBatchIntersect1xN@")]);b.select("?GPInstanceBatchIntersect1xN@");rng=random.Random(0x82bab4a8);bad=[];O=0x50000000;A=0x50002000;B=0x50003000;SEP=0x85000000;MAX=0x85000100;PRISM=0x82bb9bd8
for i in range(128):
 result=rng.randbytes(0x800);gp1=bytearray(rng.randbytes(0x100));gp2=bytearray(rng.randbytes(0x100));direction=struct.unpack(">4I",struct.pack(">4f",*(rng.uniform(-3,3) for _ in range(4))))
 for gp in [gp1,gp2]:struct.pack_into(">f",gp,0x80,1.0);struct.pack_into(">I",gp,0x90,1);struct.pack_into(">I",gp,0xa4,MAX)
 results=[]
 for compiled in (False,True):
  c=F.cpu(b);c.mem.add(O,0x800,result);c.mem.add(A,0x100,gp1);c.mem.add(B,0x100,gp2);trace=[]
  if compiled:table=next(a for a,n in b.unresolved.items() if "gapFindBestSeparatingDirection" in n)
  else:c.mem.add(0x82f91800,0x100);table=0x82f91800
  c.mem.wr(table,struct.pack(">I",SEP)*36)
  def separate(cpu):
   trace.append(("sep",tuple(cpu.g(k)&0xffffffff for k in range(3,6))));cpu.mem.wr(cpu.g(3)&0xffffffff,struct.pack(">4I",*direction));cpu.vr[1]=struct.unpack(">4I",struct.pack(">4f",-1,-1,-1,-1))
  def maximum(cpu):trace.append(("max",tuple(cpu.g(k)&0xffffffff for k in range(3,6)),tuple(cpu.vr[1])))
  def prism(cpu):trace.append(("prism",tuple(cpu.g(k)&0xffffffff for k in range(3,7))));cpu.setg(3,0)
  c.hooks[SEP]=separate;c.hooks[MAX]=maximum
  if compiled:
   for a,n in b.unresolved.items():
    if "FindFeatureIntersectionPrism" in n:c.hooks[a]=prism
  else:c.hooks[PRISM]=prism
  c.setg(3,O);c.setg(4,1);c.setg(5,A);c.setg(6,B);c.setg(7,1);c.setf(1,.125);c.run(b.entry if compiled else 0x82bab4a8)
  # Stack-local prism receiver differs between compilers; normalize that one pointer only.
  trace=[(x[0],(0,)+x[1][1:]) if x[0]=="prism" else x for x in trace];results.append((c.mem.rd(O,0x800),c.mem.rd(A,0x100),c.mem.rd(B,0x100),c.g(3)&0xffffffff,trace))
 if results[0]!=results[1]:bad.append({"case":i,"original_trace":results[0][-1],"compiled_trace":results[1][-1],"original_return":results[0][3],"compiled_return":results[1][3],"different_regions":[k for k in range(3) if results[0][k]!=results[1][k]]})
print("cases",128,"mismatches",len(bad),bad[:2]);(out/"gp-member-dispatch-1xn-nan-v5-comparison.json").write_text(json.dumps({"cases":128,"mismatches":bad,"object_sha256":hashlib.sha256(obj.read_bytes()).hexdigest(),"scope":"actual whole GPInstanceBatchIntersect1xN against ARTIST 82BAACD8; controlled separator/GP member/prism services, receiver, ccw, v1 raw lanes and all external buffers; stack-local prism pointer normalized, no service behavior claim"},indent=2))
