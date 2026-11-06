from pathlib import Path
import hashlib,json,random,struct,sys
parent=Path(r'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(parent/'scratch/x360_sdk'));sys.path.insert(0,str(parent/'tools/re'))
import native_atomic_fixture as F
import x360rd as R
out=parent/'b5-decomp/build/x360/parallel/native_textures/renderer-checkpoint1';r=json.loads((out/'compile-results.json').read_text())['results'][0];p=Path(r['object']);bundle=F.Bundle([F.Coff(p,'?Prepare@BrnRendererModule@@')]);bundle.select('?Prepare@BrnRendererModule@@')
def branch(at):
 w=R.u32(at);off=w&0x03fffffc;return (at+off-(1<<26 if off&(1<<25) else 0))&0xffffffff
def absolute(high,low):
 h=R.u32(high)&0xffff;l=R.u32(low)&0xffff;return ((h<<16)+(l-(0x10000 if l&0x8000 else 0)))&0xffffffff
originals={name:branch(at) for name,at in [('base',0x82409C8C),('allocator',0x82409D0C),('2d',0x82409D2C),('3d',0x82409D70),('untex',0x82409DB0),('sky',0x82409F58),('register',0x82409F64)]}
debug_global=absolute(0x82409DDC,0x82409DE0);SELF=F.M.S.SELF;AUX=F.M.S.AUX;rng=random.Random(0x82409C20);errors=[]
for i in range(192):
 image=rng.randbytes(0xca80);stage=[0,1,2,3,4,5,0xffffffff,0x80000000][i%8];base_ready=bool(i%3);fail_mask=(i//3)&127;mutated_stage=[None,0,4,0x7fffffff,0xffffffff,0x80000000][(i//16)%6];both=[]
 for compiled in [False,True]:
  c=F.cpu(bundle);c.mem.add(0x82f2c000,0x2000);c.mem.add(debug_global&~0xfff,0x1000);c.mem.wr(SELF,image);F.put(c,SELF+0x228,stage);F.put(c,debug_global,AUX+0x3000);c.setg(3,SELF);c.setg(4,AUX+0x1000);trace=[];num_buffers=[0]
  def snapshot():return hashlib.sha256(c.mem.rd(SELF,0xca80)).hexdigest()
  def module(x):
   trace.append(('module',x.g(3)&0xffffffff,snapshot()))
   if mutated_stage is not None:F.put(x,SELF+0x228,mutated_stage)
   x.setg(3,base_ready)
  def allocator(x):trace.append(('allocator',x.g(3)&0xffffffff,x.g(4)&0xffffffff));x.setg(3,AUX+0x3000)
  def prepare(kind):
   def run(x):
    n=num_buffers[0];num_buffers[0]+=1;args=[x.g(r)&0xffffffff for r in range(3,8)];trace.append((kind,args,snapshot()));x.mem.wr(args[0],bytes([n+1])*4);x.setg(3,not (fail_mask&(1<<n)))
   return run
  def sky(x):
   a=[x.g(r)&0xffffffff for r in range(3,6)];trace.append(('sky',a,snapshot()));x.mem.wr(a[0],b'\x12\x34\x56\x78');x.setg(3,i%2)
  def register(x):trace.append(('register',x.g(3)&0xffffffff,snapshot()))
  def memory(x):trace.append(('memory',x.g(3)&0xffffffff,snapshot()))
  def begin(x):trace.append(('assert_begin',))
  def fire(x):trace.append(('assert',F.cstring(x,x.g(3)&0xffffffff),F.cstring(x,x.g(4)&0xffffffff),x.g(5)&0xffffffff))
  def end(x):trace.append(('assert_end',))
  for name,fn in [('base',module),('allocator',allocator),('2d',prepare('2d')),('3d',prepare('3d')),('untex',prepare('untex')),('sky',sky),('register',register)]:c.hooks[originals[name]]=fn
  c.hooks[0x82817548]=begin;c.hooks[0x82820810]=fire;c.hooks[0x82817558]=end;c.hooks[0x823FBDE0]=memory;c.hooks[F.address(bundle,'?MemoryUsage@BrnRendererModule@@')]=memory
  for at,name in bundle.unresolved.items():
   if name.startswith('?Prepare@ModuleSingleBuffered@'):c.hooks[at]=module
   elif name.startswith('?GetRWGeneralResourceAllocator@'):c.hooks[at]=allocator
   elif name.startswith('?Prepare@?$ImRenderBuffer@UBasic2d'):c.hooks[at]=prepare('2d')
   elif name.startswith('?Prepare@?$ImRenderBuffer@UBasicColouredTextured'):c.hooks[at]=prepare('3d')
   elif name.startswith('?Prepare@?$ImRenderBuffer@UBasicColouredVertex'):c.hooks[at]=prepare('untex')
   elif name.startswith('?GetDebugAllocator@'):c.hooks[at]=lambda x:x.setg(3,0x82F2C7DC)
   elif name.startswith('?GetGraphicsAllocator@'):c.hooks[at]=lambda x:x.setg(3,0x82F2C814)
   elif name.startswith('?Prepare@BrnSkyDomeManager@@'):c.hooks[at]=sky
   elif name.startswith('?Register@DebugComponent@'):c.hooks[at]=register
   elif name.startswith('?BeginAssert@'):c.hooks[at]=begin
   elif name.startswith('?FireAssert@'):c.hooks[at]=fire
   elif name.startswith('?EndAssert@'):c.hooks[at]=end
  c.run(bundle.entry if compiled else 0x82409C20);both.append((c.g(3)&0xffffffff,c.mem.rd(SELF,0xca80),trace))
 if both[0]!=both[1]:errors.append(dict(case=i,stage=hex(stage),base_ready=base_ready,mutation=mutated_stage,different_bytes=[hex(n) for n,(a,b) in enumerate(zip(both[0][1],both[1][1])) if a!=b],returns=[x[0] for x in both],traces=[x[2] for x in both]))
result=dict(cases=192,errors=errors,object=str(p),sha256=hashlib.sha256(p.read_bytes()).hexdigest(),original_services={n:hex(a) for n,a in originals.items()},debug_allocator_presence_global=hex(debug_global),scope='whole actual Renderer::Prepare versus ARTIST82409C20:allstages,manager retries and enum mutations,seven stream size/allocator/grace flags,assert sequence/expression/source/line,registration/report order,and allCA80receiverbytes with call snapshots',limits='allocator lookup,valid debug allocator,buffer/Sky preparation,debug registration and MemoryUsage are controlled services; those implementations and physical resources are separate work')
(out/'renderer-prepare-comparison.json').write_text(json.dumps(result,indent=2)+'\n');print('192 native Prepare comparisons; errors',len(errors));print([{k:v for k,v in e.items() if k!='traces'} for e in errors[:3]]);raise SystemExit(bool(errors))
