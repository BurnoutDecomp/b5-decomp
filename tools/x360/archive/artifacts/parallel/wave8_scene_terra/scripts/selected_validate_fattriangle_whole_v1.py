import sys,random,json,struct,hashlib
from pathlib import Path
sys.path.insert(0,r"C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\scratch\x360_sdk");import native_atomic_fixture as F
class CPU(F.AtomicCPU):
 def step(self,pc,w,nxt,depth):
  if pc in getattr(self,"rest",set()):raise F.CPU.run.__globals__["Return"]()
  if w>>26==8:
   d,a=(w>>21)&31,(w>>16)&31;im=w&65535
   if im&32768:im-=65536
   mask=0xffffffffffffffff;v=(im&mask)+((~self.g(a))&mask)+1;self.xer_ca=int(v>mask);self.setg(d,v&mask);return nxt
  if w>>26==31 and ((w>>1)&1023)==136:
   d,a,r=(w>>21)&31,(w>>16)&31,(w>>11)&31;mask=0xffffffffffffffff;v=((~self.g(a))&mask)+self.g(r)+self.xer_ca;self.xer_ca=int(v>mask);self.setg(d,v&mask);return nxt
  if w>>26==31 and ((w>>1)&1023)==444 and w&1:
   x=(w>>21)&31;d=(w>>16)&31;y=(w>>11)&31;v=self.g(x)|self.g(y);self.setg(d,v);self.setcr(0,bool(v>>63),bool(v and not v>>63),v==0);return nxt
  return super().step(pc,w,nxt,depth)
out=Path(r"D:\Reverse\BP-X360-Experimental-Artifacts\parallel\wave8_scene_terra\volume-baseline");paths=[out/"closure-selected-v1/LineSegIntersect.obj",out/"closure-selected-v1/GPTriangle.obj",out/"closure-selected-v1/AALineClipper.obj"];b=F.Bundle([F.Coff(paths[0],"?FatTriangleLineSegIntersect@"),F.Coff(paths[1],"?TriangleNearestPointRegion@"),F.Coff(paths[2],"?Init@AALineClipper@")]);b.select("?FatTriangleLineSegIntersect@");rng=random.Random(0x82bbad98);bad=[];errors=[];counts={"hit":0,"miss":0}
for i in range(192):
 initial=bytearray(rng.randbytes(0x100));struct.pack_into(">4f",initial,0x20,0,0,1,0);start=[rng.uniform(-3,3) for _ in range(4)];delta=[rng.uniform(-3,3) for _ in range(4)];rows=[start,delta,[-1,-1,0,0],[1,-1,0,0],[0,1,0,0]];vectors=[struct.unpack(">4I",struct.pack(">4f",*v)) for v in rows];results=[]
 for compiled in (False,True):
  c=F.cpu(b);c.__class__=CPU;c.rest=set();c.mem.add(0x50000000,0x100,initial)
  for addr,name in b.unresolved.items():
   if name.startswith("__save"):c.hooks[addr]=lambda cpu:None
   elif name.startswith("__rest"):c.rest.add(addr);c.hooks[addr]=lambda cpu:None
  c.setg(3,0x50000000);c.setf(1,.3)
  for k,v in enumerate(vectors):c.vr[k+1]=v
  try:c.run(b.entry if compiled else 0x82bbad98)
  except Exception as ex:errors.append({"case":i,"compiled":compiled,"error":repr(ex)});break
  results.append((c.g(3)&0xffffffff,c.mem.rd(0x50000000,0x100)))
 if len(results)<2:continue
 counts["hit" if results[0][0] else "miss"]+=1
 if results[0]!=results[1]:
  diff=[{"offset":j,"original":results[0][1][j:j+4].hex(),"compiled":results[1][1][j:j+4].hex()} for j in range(0,0x100,4) if results[0][1][j:j+4]!=results[1][1][j:j+4]];bad.append({"case":i,"original_return":results[0][0],"compiled_return":results[1][0],"diff":diff[:10]})
print("counts",counts,"mismatches",len(bad),"errors",len(errors),bad[:3],errors[:1]);(out/"fattriangle-whole-v1-final-selected-comparison.json").write_text(json.dumps({"counts":counts,"mismatches":bad,"errors":errors,"objects":{p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in paths},"scope":"actual whole FatTriangle and real nearest provider vs original, fixed triangle, finite arbitrary xyzw lines and full result guards; model limitations, no broad claim"},indent=2))
