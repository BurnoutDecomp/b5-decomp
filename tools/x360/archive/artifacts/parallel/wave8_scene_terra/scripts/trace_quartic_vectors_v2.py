import sys,random,json,struct,hashlib
from pathlib import Path
sys.path.insert(0,r"C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\scratch\x360_sdk");import native_atomic_fixture as F
import importlib.util
spec=importlib.util.spec_from_file_location("cylmodel",r"D:\Reverse\BP-Decomp_Workflow\scratch\OWNERLIST_0927\L2\emu\gen_cylinder_line.py");cylmodel=importlib.util.module_from_spec(spec);spec.loader.exec_module(cylmodel)
class CPU(F.AtomicCPU,cylmodel.Cpu):
 def exec_vector(self,pc,w,mn):
  if getattr(self,"debug_deriv",False) and pc in [0x82bad358,0x82bad368,0x82bad370,0x82bad378,0x82bad380]:print("V",hex(pc),[float(self.vf(k)[0]) for k in [0,5,10,28,30,63,95]],flush=True)
  return super().exec_vector(pc,w,mn)
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
out=Path(r"D:\Reverse\BP-X360-Experimental-Artifacts\parallel\wave8_scene_terra\volume-baseline");obj=out/"LineSeg-current-v8.obj";b=F.Bundle([F.Coff(obj,"?SolveQuarticRoots@")]);b.select("?SolveQuarticRoots@");rng=random.Random(0x82baca30);bad=[];errors=[];counts={"accepted":0,"rejected":0}
for i in range(2):
 coefficients=[rng.uniform(-3,3),rng.uniform(.5,2),rng.uniform(-1,1),rng.uniform(-.3,.3),rng.uniform(.01,.2)];coef=struct.pack(">5f",*coefficients)+rng.randbytes(44);initial=rng.randbytes(64);results=[]
 for compiled in (False,True):
  c=F.cpu(b);c.__class__=CPU;c.site_seen=[0]*len(cylmodel.SITES);c.rest=set();c.max_steps=1000000;c.mem.add(0x50000000,64,coef);c.mem.add(0x50001000,64,initial)
  for addr,name in b.unresolved.items():
   if name.startswith("__save"):c.hooks[addr]=lambda cpu:None
   elif name.startswith("__rest"):c.rest.add(addr);c.hooks[addr]=lambda cpu:None
  if i==1:
   def observe(pc,w):
    if (not compiled and pc==0x82bad3ac):print("ORIG",c.f(0),c.f(11),c.f(29),c.f(30),flush=True)
    if (compiled and pc==b.entry+0x288):print("NEW ",c.f(29),c.f(11),c.f(12),c.f(13),flush=True)
   c.instruction_observer=observe
  c.debug_deriv=(i==1 and not compiled)
  c.setg(3,0x50000000);c.setg(4,0x50001000)
  try:c.run(b.entry if compiled else 0x82baca30)
  except Exception as ex:errors.append({"case":i,"compiled":compiled,"error":repr(ex)});break
  results.append((c.g(3)&0xffffffff,c.mem.rd(0x50001000,64),c.mem.rd(0x50000000,64)))
 if len(results)<2:continue
 counts["accepted" if results[0][0] else "rejected"]+=1
 if results[0]!=results[1]:bad.append({"case":i,"coef":coef[:20].hex(),"original_return":results[0][0],"compiled_return":results[1][0],"original_root":results[0][1][:4].hex(),"compiled_root":results[1][1][:4].hex()})
print("counts",counts,"mismatches",len(bad),"errors",len(errors),bad[:2],errors[:2]);(out/"quartic-native-trace-v1.json").write_text(json.dumps({"counts":counts,"mismatches":bad,"errors":errors,"object_sha256":hashlib.sha256(obj.read_bytes()).hexdigest(),"scope":"actual whole SolveQuartic vs ARTIST 82BACA30, finite polynomial coefficients, complete coefficient/root guards; emulator vector log/exp/estimate model; no hardware claim"},indent=2))
