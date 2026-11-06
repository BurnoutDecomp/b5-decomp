from pathlib import Path
import sys,json,random,struct,hashlib
sys.path.insert(0,r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\scratch\x360_sdk');import native_atomic_fixture as F
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/parallel/wave8_scene_terra/mask4-consumer-v1');base=out.parent;ln='?LineTestRecursive@LooseOctree@CgsSceneManager@@';sn='?TestLineSphere4@CgsGeometric@@YA?BVMask4@vpu@math@rw@@';cn='?CalcEntityIndex@SpatialPartition@CgsSceneManager@@';gn='?GetEntityBoundingSphereConst@SpatialPartition@CgsSceneManager@@';pn='?PushResult@?$CoarseQueryResultBuffer@$0EAAA@@CgsSceneManager@@';oct=out/'2.obj';lines=out/'0.obj';spatial=base/'committed-build/6.obj';b=F.Bundle([F.Coff(oct,ln),F.Coff(lines,sn),F.Coff(spatial,cn),F.Coff(spatial,gn)]);b.select(ln);push=F.address(b,pn);A=0x40000000;N=A+0x46000;P=A+0x48000;rng=random.Random(0x828BCF50);bad=[]
class Cpu(F.AtomicCPU):
 def step(self,pc,word,nxt,depth):
  if word>>26==8:
   d=(word>>21)&31;ra=(word>>16)&31;imm=word&65535;imm=imm-65536 if imm&32768 else imm;v=(imm&0xffffffffffffffff)+((~self.g(ra))&0xffffffffffffffff)+1;self.setg(d,v&0xffffffffffffffff);self.xer_ca=int(v>>64);return nxt
  if word>>26==31 and ((word>>1)&1023)==136:
   d=(word>>21)&31;ra=(word>>16)&31;rb=(word>>11)&31;v=((~self.g(ra))&0xffffffffffffffff)+self.g(rb)+getattr(self,'xer_ca',0);self.setg(d,v&0xffffffffffffffff);self.xer_ca=int(v>>64);return nxt
  return super().step(pc,word,nxt,depth)
def run(comp,spheres,start,direction,length):
 c=F.cpu(b);c.__class__=Cpu;c.mem.add(A,0x50000);c.mem.wr(A+0x446a4,N.to_bytes(4,'big'));c.mem.wr(N+0x42,b'\xff\xff');c.mem.wr(N+0x44,(4).to_bytes(4,'big'));c.mem.wr(N+0x48,(A+0x80).to_bytes(4,'big'));c.mem.wr(N+0x4c,b'\x00\x00');c.mem.wr(N+0x54,(1).to_bytes(4,'big'))
 for i,x in enumerate(spheres):
  c.mem.wr(A+0x80+8*i,(1).to_bytes(4,'big')+((i+1) if i<3 else 0xffff).to_bytes(2,'big')+b'\xff\xff');c.mem.wr(A+0x13900+16*i,struct.pack('>4f',*x))
 c.mem.wr(P,struct.pack('>4f',*start));c.mem.wr(P+0x20,struct.pack('>4f',*direction));c.mem.wr(P+0x40,struct.pack('>4f',*[length]*4));c.mem.wr(P+0x50,(1).to_bytes(4,'big'));c.mem.wr(P+0x54,(A+0x49000).to_bytes(4,'big'));calls=[];fires=[]
 def pushfn(cpu):calls.append([cpu.g(3)&0xffffffff,cpu.g(4)&0xffff]);cpu.setg(3,1)
 def fire(cpu):fires.append(1)
 c.hooks[0x828ADAC0]=pushfn;c.hooks[push]=pushfn
 for nm,addr in [('BeginAssert',0x82817548),('FireAssert',0x82820810),('EndAssert',0x82817558)]:
  fn=fire if nm=='FireAssert' else lambda cpu:None;c.hooks[addr]=fn
  for k,v in b.unresolved.items():
   if nm in v:c.hooks[k]=fn
 for k,v in b.unresolved.items():
  if v.startswith('__save') or v.startswith('__rest'):c.hooks[k]=lambda cpu:None
 c.setg(3,A);c.setg(4,0);c.setg(5,P);c.run(b.entry if comp else 0x828BCF50);return calls,len(fires)
for i in range(128):
 spheres=[[rng.uniform(-5,5) for j in range(3)]+[rng.uniform(.1,3)] for k in range(4)];start=[rng.uniform(-5,5) for j in range(4)];direction=[1.,0.,0.,rng.uniform(-5,5)];length=rng.uniform(.1,10);x=run(False,spheres,start,direction,length);y=run(True,spheres,start,direction,length)
 if x!=y:bad.append({'case':i,'original':x,'compiled':y})
r={'cases':128,'mismatches':bad,'scope':'actual original LooseOctree::LineTestRecursive828BCF50 vs linked compiled actual octree caller + genuine Mask4 TestLineSphere4 + actual spatial helpers; one node, four linked sphere entities, no children, finite X-axis lines and W; compares ordered real PushResult receiver/entity calls with PushResult and assertions controlled. No traversal/partial batches/full class ABI/hardware claim; emulator local subfic/subfe carry support','octree_object_sha256':hashlib.sha256(oct.read_bytes()).hexdigest(),'line_object_sha256':hashlib.sha256(lines.read_bytes()).hexdigest(),'spatial_object_sha256':hashlib.sha256(spatial.read_bytes()).hexdigest()};(out/'octree-sphere-caller-comparison.json').write_text(json.dumps(r,indent=2)+'\n');print('cases',128,'mismatches',len(bad),bad[:1]);assert not bad
