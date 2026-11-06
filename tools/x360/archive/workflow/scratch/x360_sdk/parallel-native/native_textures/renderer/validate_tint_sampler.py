from pathlib import Path
import hashlib,json,random,struct,sys
parent=Path(r"C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow")
sys.path.insert(0,str(parent/"scratch/x360_sdk"));import native_atomic_fixture as F
sys.path.insert(0,str(parent/"tools/re"));import x360rd
out=parent/"b5-decomp/build/x360/parallel/native_textures/sampler-callers4"
p=Path(next(x["object"] for x in json.loads((out/"compile-results.json").read_text())["results"] if x["source"].endswith("rwgpfxtint.cpp")))
bundle=F.Bundle([F.Coff(p,"?Initialize@Tint@")]);bundle.select("?Initialize@Tint@");SELF=F.M.S.SELF;AUX=F.M.S.AUX
class Stop(Exception):pass
def target(pc):
 w=x360rd.u32(pc);d=w&0x3fffffc
 if d&0x2000000:d-=0x4000000
 return (pc+d)&0xffffffff
errors=[];rng=random.Random(0x82403cd4)
for i in range(128):
 image=rng.randbytes(0x100);both=[];size=[0,1,2,8,16,64,0x80000000,0xffffffff][i%8];texture=0 if i%9==0 else AUX+0x7000+i*4
 for compiled in [False,True]:
  c=F.cpu(bundle);c.mem.wr(SELF,image);F.put(c,AUX,SELF);F.put(c,AUX+0x100,size);F.put(c,AUX+0x104,AUX+0x1000);F.put(c,AUX+0x1000,AUX+0x1100);F.put(c,AUX+0x1110,0x84803000)
  sp=c.g(1)&0xffffffff;c.mem.wr(sp-0x2000,bytes([i])*0x2000);c.setg(3,AUX);c.setg(4,AUX+0x100);trace=[]
  def descriptor(x):
   trace.append(("texture-descriptor",x.mem.rd(x.g(4)&0xffffffff,28).hex()));x.mem.wr(x.g(3)&0xffffffff,struct.pack(">10I",1,2,3,4,5,6,7,8,9,10))
  def allocate(x):
   trace.append(("allocate",x.g(4)&0xffffffff,x.mem.rd(x.g(5)&0xffffffff,40).hex(),x.g(6)&0xffffffff));x.mem.wr(x.g(3)&0xffffffff,struct.pack(">5I",*(0x45670000+i*8+len(trace)+j for j in range(5))))
  def texture_init(x):
   trace.append(("texture-initialize",x.mem.rd(x.g(3)&0xffffffff,20).hex(),x.mem.rd(x.g(4)&0xffffffff,28).hex()));x.setg(3,texture)
  def state_descriptor(x):
   trace.append(("state-descriptor",));x.mem.wr(x.g(3)&0xffffffff,struct.pack(">10I",36,4,0,1,0,1,0,1,0,1))
  def state_init(x):
   trace.append(("state-initialize",x.mem.rd(x.g(3)&0xffffffff,20).hex(),x.mem.rd(x.g(4)&0xffffffff,76).hex()));raise Stop()
  c.hooks[0x84803000]=allocate
  for at,fn in [(target(0x82403bd8),descriptor),(target(0x82403c4c),texture_init),(target(0x82403cf0),state_descriptor),(target(0x82403d64),state_init)]:c.hooks[at]=fn
  for at,name in bundle.unresolved.items():
   if name.startswith("?GetResourceDescriptor@Texture@"):c.hooks[at]=descriptor
   elif name.startswith("?Initialize@Texture@"):c.hooks[at]=texture_init
   elif name.startswith("?GetResourceDescriptor@TextureState@"):c.hooks[at]=state_descriptor
   elif name.startswith("?Initialize@TextureState@"):c.hooks[at]=state_init
   elif name.startswith("??0BaseResourceDescriptor@"):c.hooks[at]=lambda x:x.mem.wr(x.g(3)&0xffffffff,struct.pack(">II",0,1))
  try:c.run(bundle.entry if compiled else 0x82403b48)
  except Stop:pass
  assert trace[-1][0]=="state-initialize"
  both.append((c.mem.rd(SELF,0x100),trace))
 if both[0]!=both[1]:errors.append(dict(case=i,byte_differences=[hex(n) for n,(a,b) in enumerate(zip(both[0][0],both[1][0])) if a!=b],traces=[x[1] for x in both]))
result=dict(cases=128,errors=errors,object=str(p),sha256=hashlib.sha256(p.read_bytes()).hexdigest(),scope="Actual whole-TU Tint::Initialize prefix through sampler Initialize; all 76 sampler parameter bytes, padding, mip override 0, both five-lane allocation copies and all 256 Tint bytes at that boundary.",limits="Stops at sampler Initialize; external descriptor, allocator and texture Initialize operations are controlled. No full Tint behavior claim.")
(out/"tint-sampler-comparison.json").write_text(json.dumps(result,indent=2)+"\n");print("128 Tint sampler cases; errors",len(errors));print(errors[:2]);raise SystemExit(bool(errors))
