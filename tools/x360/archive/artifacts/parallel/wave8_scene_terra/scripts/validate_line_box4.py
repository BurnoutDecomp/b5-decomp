from pathlib import Path
import sys,json,random,struct,hashlib
sys.path.insert(0,r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\scratch\x360_sdk');import native_atomic_fixture as F
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/parallel/wave8_scene_terra/mask4-consumer-v1');obj=out/'0.obj';n='?TestLineBoundingBoxAgainstAxisAlignedBox4@CgsGeometric@@YA?BVMask4@vpu@math@rw@@';b=F.Bundle([F.Coff(obj,n)]);b.select(n);A=0x40000000;rng=random.Random(0x828BD908);bad=[]
class Stop(Exception):pass
class Cpu(F.AtomicCPU):
 def step(self,pc,word,nxt,depth):
  if word>>26==8:
   d=(word>>21)&31;ra=(word>>16)&31;imm=word&65535;imm=imm-65536 if imm&32768 else imm;v=(imm&0xffffffffffffffff)+((~self.g(ra))&0xffffffffffffffff)+1;self.setg(d,v&0xffffffffffffffff);self.xer_ca=int(v>>64);return nxt
  if word>>26==31 and ((word>>1)&1023)==136:
   d=(word>>21)&31;ra=(word>>16)&31;rb=(word>>11)&31;v=((~self.g(ra))&0xffffffffffffffff)+self.g(rb)+getattr(self,'xer_ca',0);self.setg(d,v&0xffffffffffffffff);self.xer_ca=int(v>>64);return nxt
  return super().step(pc,word,nxt,depth)
def vec(x):return struct.unpack('>4I',struct.pack('>4f',*x))
def run(comp,boxes,start,recip):
 c=F.cpu(b);c.__class__=Cpu;c.mem.add(A,0x1000)
 for i,(lo,hi) in enumerate(boxes):c.mem.wr(A+0x100+i*32,struct.pack('>8f',*lo,*hi))
 if comp:
  for addr,name in b.unresolved.items():
   if name.startswith('__save') or name.startswith('__rest'):c.hooks[addr]=lambda cpu:None
  for i in range(4):c.setg(3+i,A+0x100+i*32)
  c.vr[1]=vec(start);c.vr[2]=vec([0.]*4);c.vr[3]=vec(recip);c.run(b.entry);return list(c.vr[1])
 for i,(lo,hi) in enumerate(boxes):c.vr[116+i*2]=vec(lo);c.vr[117+i*2]=vec(hi)
 c.vr[127]=vec(start);c.vr[12]=vec(recip);c.vr[125]=vec([1.]*4);c.vr[126]=vec([0.]*4);c.vr[124]=(0xffffffff,)*4;c.setg(27,0)
 def obs(pc,word):
  if pc==0x828BD9F8:raise Stop()
 c.instruction_observer=obs
 try:c.run(0x828BD908)
 except Stop:pass
 return list(c.vr[127])
for i in range(128):
 boxes=[]
 for j in range(4):
  lo=[rng.uniform(-8,0) for k in range(3)]+[rng.uniform(-8,8)];hi=[rng.uniform(0,8) for k in range(3)]+[rng.uniform(-8,8)];boxes.append((lo,hi))
 start=[rng.uniform(-10,10) for j in range(4)];recip=[rng.choice([-1.,1.])*rng.uniform(.1,3) for j in range(4)];x=run(False,boxes,start,recip);y=run(True,boxes,start,recip)
 if x!=y:bad.append({'case':i,'original':x,'compiled':y})
r={'cases':128,'mismatches':bad,'scope':'original retail inlined four-box walk828BD908..9EC vs compiled genuine Mask4 TestLineBoundingBoxAgainstAxisAlignedBox4; four finite random boxes/start/reciprocal incl W, nonzero reciprocal, all enabled lanes; compares all v1/v127 mask bits. Asserts/traversal and exceptional inputs excluded; emulator implements subfic/subfe carry locally','object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest()};(out/'line-box4-comparison.json').write_text(json.dumps(r,indent=2)+'\n');print('cases',128,'mismatches',len(bad),bad[:1]);assert not bad
