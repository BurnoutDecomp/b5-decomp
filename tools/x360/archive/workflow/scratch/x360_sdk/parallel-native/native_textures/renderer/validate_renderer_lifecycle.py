from pathlib import Path
import hashlib,json,random,struct,sys
parent=Path(r'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(parent/'scratch/x360_sdk'));sys.path.insert(0,str(parent/'tools/re'));import native_atomic_fixture as F;import x360rd as R
out=parent/'b5-decomp/build/x360/parallel/native_textures/renderer-release-destruct1';p=Path(next(x['object'] for x in json.loads((out/'compile-results.json').read_text())['results'] if x['source'].endswith('BrnRendererModule.cpp')));bundle=F.Bundle([F.Coff(p,'?Release@BrnRendererModule@@')]);SELF=F.M.S.SELF;AUX=F.M.S.AUX;rng=random.Random(0x823FBCE0);errors=[]
def branch(at):
 w=R.u32(at);off=w&0x03fffffc;return (at+off-(1<<26 if off&(1<<25) else 0))&0xffffffff
bundle.select('?Release@BrnRendererModule@@')
for i in range(192):
 image=rng.randbytes(0xca80);stage=[0,1,2,3,4,5,0xffffffff,0x80000000][i%8];corona_ready=bool((i//8)%3);base_ready=bool((i//24)%3);mutation=[None,0,4,0x7fffffff,0xffffffff,0x80000000][(i//16)%6];both=[]
 for compiled in [False,True]:
  c=F.cpu(bundle);c.mem.wr(SELF,image);F.put(c,SELF+0x22c,stage);c.setg(3,SELF);trace=[]
  def snapshot():return hashlib.sha256(c.mem.rd(SELF,0xca80)).hexdigest()
  def corona(x):
   trace.append(('corona',x.g(3)&0xffffffff,x.g(4)&0xffffffff,snapshot()))
   if mutation is not None:F.put(x,SELF+0x22c,mutation)
   x.setg(3,(0xabc123<<8)|corona_ready)
  def base(x):trace.append(('base',x.g(3)&0xffffffff,snapshot()));x.setg(3,(0xcde321<<8)|base_ready)
  def begin(x):trace.append(('begin',))
  def fire(x):trace.append(('assert',F.cstring(x,x.g(3)&0xffffffff),F.cstring(x,x.g(4)&0xffffffff),x.g(5)&0xffffffff))
  def end(x):trace.append(('end',))
  c.hooks[branch(0x823FBD4C)]=corona;c.hooks[branch(0x823FBD78)]=base;c.hooks[0x82817548]=begin;c.hooks[0x82820810]=fire;c.hooks[0x82817558]=end
  for at,name in bundle.unresolved.items():
   if name.startswith('?Release@BrnCoronaManager@@'):c.hooks[at]=corona
   elif name.startswith('?Release@ModuleSingleBuffered@'):c.hooks[at]=base
   elif name.startswith('?GetGraphicsAllocator@'):c.hooks[at]=lambda x:x.setg(3,0x82F2C814)
   elif name.startswith('?BeginAssert@'):c.hooks[at]=begin
   elif name.startswith('?FireAssert@'):c.hooks[at]=fire
   elif name.startswith('?EndAssert@'):c.hooks[at]=end
  c.run(bundle.entry if compiled else 0x823FBCE0);both.append((c.g(3)&0xffffffff,c.mem.rd(SELF,0xca80),trace))
 if both[0]!=both[1]:errors.append(dict(method='Release',case=i,stage=hex(stage),mutation=mutation,returns=[x[0] for x in both],bytes=[hex(n) for n,(a,b) in enumerate(zip(both[0][1],both[1][1])) if a!=b],traces=[x[2] for x in both]))
bundle.select('?Destruct@BrnRendererModule@@')
for (oi,sec),base in bundle.bases.items():
 obj=bundle.objects[oi];body=bundle.images[base]
 for at,index,kind in obj.relocations(sec):
  name,_,_=obj.symbols[index]
  if not name.startswith('?msPostFx@@'):continue
  address=0x82FAF0F0;w=struct.unpack_from('>I',body,at)[0]
  if kind in [0x10,0x11]:w=(w&0xffff0000)|(((address+0x8000)>>16 if kind==0x10 else address)&0xffff)
  elif kind==2:w=address
  else:raise AssertionError((name,kind))
  struct.pack_into('>I',body,at,w)
for i in range(128):
 image=rng.randbytes(0xca80);backend=rng.randbytes(0x98);other=rng.randbytes(0x98);both=[]
 for compiled in [False,True]:
  c=F.cpu(bundle);c.mem.wr(SELF,image);c.mem.wr(AUX+0x6000,backend);c.mem.wr(AUX+0x6200,other);F.put(c,SELF+0x24c,AUX+0x4000);F.put(c,AUX+0x4108,0 if i%4==0 else AUX+0x6000);F.put(c,SELF+0x394c,AUX+0x1000)
  for alloc in [AUX+0x1000,AUX+0x1200]:F.put(c,alloc,AUX+0x2000)
  F.put(c,AUX+0x2014,0x84803000);c.setg(3,SELF);trace=[]
  def snapshot():return (hashlib.sha256(c.mem.rd(SELF,0xca80)).hexdigest(),hashlib.sha256(c.mem.rd(AUX+0x6000,0x98)).hexdigest(),hashlib.sha256(c.mem.rd(AUX+0x6200,0x98)).hexdigest())
  def sun(x):
   trace.append(('sun',x.g(3)&0xffffffff,snapshot()))
   if i%3==1:F.put(x,SELF+0x394c,AUX+0x1200);F.put(x,AUX+0x4108,AUX+0x6200)
  def free(x):
   trace.append(('free',x.g(3)&0xffffffff,x.g(4)&0xffffffff,x.mem.rd(x.g(4)&0xffffffff,20).hex(),snapshot()))
   if sum(t[0]=='free' for t in trace)==1:
    if i%5==1:F.put(x,SELF+0x394c,AUX+0x1000)
    if i%5==2:F.put(x,SELF+0x16ac,0)
    if i%5==3:F.put(x,SELF+0x16ac,0x76543210)
  def postfx(x):trace.append(('postfx',x.g(3)&0xffffffff,snapshot()));F.put(x,SELF+0x228,i)
  c.hooks[branch(0x82408A78)]=sun;c.hooks[branch(0x82408AE0)]=postfx;c.hooks[0x84803000]=free
  for at,name in bundle.unresolved.items():
   if name.startswith('?Destruct@BrnSunCorona@@'):c.hooks[at]=sun
   elif name.startswith('?Destruct@BrnPostFx@@'):c.hooks[at]=postfx
  c.run(bundle.entry if compiled else 0x82408A58);both.append((c.mem.rd(SELF,0xca80),c.mem.rd(AUX+0x6000,0x98),c.mem.rd(AUX+0x6200,0x98),trace))
 if both[0]!=both[1]:errors.append(dict(method='Destruct',case=i,bytes=[hex(n) for n,(a,b) in enumerate(zip(both[0][0],both[1][0])) if a!=b],traces=[x[-1] for x in both]))
result=dict(cases=320,errors=errors,object=str(p),sha256=hashlib.sha256(p.read_bytes()).hexdigest(),scope='192wholeReleasecasesagainst823FBCE0 and128wholeDestructcasesagainst82408A58:allstages/retries/enumwrap/asserts,8bitreturnABI,allCA80bytes,backbufferaliases,two5laneresources,liveallocator/back/glassreadsandcleanuporder',limits='Corona/baseRelease,Sun/PostFxDestructandDoFreearecontrolledservices;noGPU/link/completeframeclaim')
(out/'renderer-lifecycle-comparison.json').write_text(json.dumps(result,indent=2)+'\n');print('320 native Renderer lifecycle cases; errors',len(errors));print(errors[:2]);raise SystemExit(bool(errors))
