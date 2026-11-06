import sys,random,json,struct,hashlib
from pathlib import Path
sys.path.insert(0,r"C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\scratch\x360_sdk");import native_atomic_fixture as F
class RunCPU(F.AtomicCPU):
 def step(self,pc,w,nxt,depth):
  if pc in getattr(self,"rest_stubs",set()):raise F.CPU.run.__globals__["Return"]()
  if w>>26==31 and ((w>>1)&1023)==444 and w&1:
   src=(w>>21)&31;dst=(w>>16)&31;rhs=(w>>11)&31;value=self.g(src)|self.g(rhs);self.setg(dst,value);self.setcr(0,bool(value>>63),bool(value and not value>>63),value==0);return nxt
  return super().step(pc,w,nxt,depth)
out=Path(r"D:\Reverse\BP-X360-Experimental-Artifacts\parallel\wave8_scene_terra\volume-baseline");paths=[out/"closure-selected-v1/LineSegIntersect.obj",out/"closure-selected-v1/GPTriangle.obj",out/"closure-selected-v1/AALineClipper.obj"];b=F.Bundle([F.Coff(paths[0],"?LineSegIntersect@BoxVolume@"),F.Coff(paths[1],"?TriangleNearestPointRegion@"),F.Coff(paths[2],"?Init@AALineClipper@")]);rng=random.Random(0x82ba9478);bad=[];fail=[];counts={};toruscalls={"original":0,"compiled":0}
for name,orig in [("?FatLineSegIntersect@CylinderVolume@",0x82baeb10)]:
 b.select(name);counts[name]={"hit":0,"miss":0}
 for i in range(96):
  volume=bytearray(rng.randbytes(0x100));transform=bytearray(rng.randbytes(0x80));initial=rng.randbytes(0x100)
  for m in [volume,transform]:
   for k,row in enumerate([(1,0,0,0),(0,1,0,0),(0,0,1,0),(0,0,0,1)]):struct.pack_into(">4f",m,16*k,*row)
  struct.pack_into(">4f",volume,0x44,2.0,1.25,1.5,.2)
  start=[rng.uniform(-5,5) for k in range(4)];end=[rng.uniform(-5,5) for k in range(4)];p1=struct.pack(">4f",*start);p2=struct.pack(">4f",*end);res=[]
  for compiled in (False,True):
   c=F.cpu(b);c.__class__=RunCPU;c.rest_stubs=set();
   for addr,img in [(0x50000000,volume),(0x50001000,p1),(0x50002000,p2),(0x50003000,transform),(0x50004000,initial)]:c.mem.add(addr,len(img),img)
   for a,n in b.unresolved.items():
    if n.startswith("__save"):c.hooks[a]=lambda cpu:None
    elif n.startswith("__rest"):c.rest_stubs.add(a);c.hooks[a]=lambda cpu:None
   trace=[]
   def torus(cpu):
    toruscalls["compiled" if compiled else "original"]+=1;trace.append((tuple(cpu.vr[1]),tuple(cpu.vr[2]),cpu.f(1),cpu.f(2)));ptr=cpu.g(3)&0xffffffff;cpu.mem.wr(ptr,struct.pack(">f",0.5));cpu.setg(3,i&1)
   c.hooks[F.address(b,"?rwcTorusLineSegIntersect@") if compiled else 0x82badab0]=torus
   c.setg(3,0x50000000);c.setg(4,0x50001000);c.setg(5,0x50002000);c.setg(6,0x50003000 if i&1 else 0);c.setg(7,0x50004000);c.setf(1,.15)
   try:c.run(b.entry if compiled else orig)
   except Exception as ex:fail.append({"function":name,"case":i,"compiled":compiled,"error":repr(ex)});break
   res.append((c.g(3)&0xffffffff,c.mem.rd(0x50004000,0x100),c.mem.rd(0x50000000,0x100),trace))
  if len(res)<2:continue
  counts[name]["hit" if res[0][0] else "miss"]+=1
  if res[0]!=res[1]:
   diffs=[{"offset":k,"original":res[0][1][k:k+4].hex(),"compiled":res[1][1][k:k+4].hex()} for k in range(0,0x100,4) if res[0][1][k:k+4]!=res[1][1][k:k+4]];bad.append({"function":name,"case":i,"orig":res[0][0],"new":res[1][0],"diffs":diffs[:8]})
 print(name,counts[name],"mismatch",sum(x["function"]==name for x in bad),"fail",sum(x["function"]==name for x in fail),flush=True)
print("torus_calls",toruscalls,"bad",bad[:2],"fail",fail[:2]);(out/"lineseg-fatcylinder-controlled-v2-final-selected-comparison.json").write_text(json.dumps({"counts":counts,"torus_calls":toruscalls,"mismatches":bad,"errors":fail,"objects":{p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in paths},"scope":"whole actual LineSeg providers vs original, finite distinct vectors, identity/null transforms; no full parity claim; failures and unsupported emulator retained"},indent=2))
