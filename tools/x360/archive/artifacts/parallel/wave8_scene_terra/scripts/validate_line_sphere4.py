from pathlib import Path
import sys,json,random,struct,hashlib
sys.path.insert(0,r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\scratch\x360_sdk');import native_atomic_fixture as F
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/parallel/wave8_scene_terra/mask4-consumer-v1');obj=out/'0.obj';n='?TestLineSphere4@CgsGeometric@@YA?BVMask4@vpu@math@rw@@';b=F.Bundle([F.Coff(obj,n)]);b.select(n);A=0x40000000;rng=random.Random(0x828BD114);bad=[]
class Stop(Exception):pass
class Cpu(F.AtomicCPU):
 def step(self,pc,word,nxt,depth):
  if word>>26==8:
   d=(word>>21)&31;ra=(word>>16)&31;imm=word&65535;imm=imm-65536 if imm&32768 else imm;v=(imm&0xffffffffffffffff)+((~self.g(ra))&0xffffffffffffffff)+1;self.setg(d,v&0xffffffffffffffff);self.xer_ca=int(v>>64);return nxt
  if word>>26==31 and ((word>>1)&1023)==136:
   d=(word>>21)&31;ra=(word>>16)&31;rb=(word>>11)&31;v=((~self.g(ra))&0xffffffffffffffff)+self.g(rb)+getattr(self,'xer_ca',0);self.setg(d,v&0xffffffffffffffff);self.xer_ca=int(v>>64);return nxt
  return super().step(pc,word,nxt,depth)
def run(comp,spheres,start,direction,length):
 c=F.cpu(b);c.__class__=Cpu;c.mem.add(A,0x1000)
 for i,x in enumerate(spheres):c.mem.wr(A+0x100+i*16,struct.pack('>4f',*x))
 v=lambda x:struct.unpack('>4I',struct.pack('>4f',*x))
 if comp:
  for i in range(4):c.setg(3+i,A+0x100+i*16)
  c.vr[1]=v(start);c.vr[2]=v(direction);c.vr[3]=v([length]*4);c.run(b.entry);return list(c.vr[1])
 sp=c.g(1)&0xffffffff
 for i,x in enumerate(spheres):c.mem.wr(sp+0xd0+i*16,struct.pack('>4f',*x))
 c.setg(21,0x82CDA3C0);c.setg(22,0x82CDA400);c.setg(27,0);c.vr[127]=v(start);c.vr[126]=v(direction);c.vr[125]=v([length]*4)
 def obs(pc,word):
  if pc==0x828BD244:raise Stop()
 c.instruction_observer=obs
 try:c.run(0x828BD114)
 except Stop:pass
 return list(c.vr[119])
for i in range(128):
 spheres=[[rng.uniform(-5,5) for j in range(3)]+[rng.uniform(.1,3)] for k in range(4)];start=[rng.uniform(-5,5) for j in range(3)]+[rng.uniform(-5,5)];direction=[1.,0.,0.,rng.uniform(-5,5)];length=rng.uniform(.1,10);x=run(False,spheres,start,direction,length);y=run(True,spheres,start,direction,length)
 if x!=y:bad.append({'case':i,'original':x,'compiled':y})
r={'cases':128,'mismatches':bad,'scope':'original retail inlined four-sphere walk828BD114..238 vs whole compiled genuine Mask4 TestLineSphere4; four finite random spheres, unit X line direction, arbitrary start/direction W and finite length; compares all v1/v119 returned mask bits. Original image permute constants and actual code slice, VMX emulator; no traversal, arbitrary direction, exceptional FP or hardware claim','object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest()};(out/'line-sphere4-comparison.json').write_text(json.dumps(r,indent=2)+'\n');print('cases',128,'mismatches',len(bad),bad[:1]);assert not bad


