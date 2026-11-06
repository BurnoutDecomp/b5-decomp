"""Compare the complete native manager-startup phase through the first fopen.

Allocator/scheduler/EAThread defaults/assert formatting/filesystem are controlled
boundaries. The actual compiled JobThreadParameters constructor executes. Worker
execution, later autotest/mount branches and physical Xenon behavior are excluded.
"""
from pathlib import Path
import hashlib,json,random,re,struct,sys
OUT=Path(__file__).resolve().parent
FIX=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/scratch/x360_sdk')
sys.path.insert(0,str(FIX));import native_atomic_fixture as F
MASK=0xffffffff;STACK=0x40000000;AUX=0x50000000;MESSAGE=AUX+0x1000;VTABLE=AUX+0x2000
class Stop(Exception):pass
class Obj:
 def __init__(self,p):
  self.path=Path(p);self.data=b=self.path.read_bytes();machine,n,_,off,count,opt,_=struct.unpack_from('<HHIIIHH',b);assert machine==0x1f2
  st=b[off+count*18:];self.sections={i+1:struct.unpack_from('<8sIIIIIIHHI',b,20+opt+i*40)for i in range(n)};self.symbols={};i=0
  while i<count:
   name,v,sec,ty,storage,aux=struct.unpack_from('<8sIhHBB',b,off+i*18)
   if name[:4]==b'\0'*4:
    at=struct.unpack_from('<I',name,4)[0];name=st[at:].split(b'\0',1)[0]
   self.symbols[i]=(name.split(b'\0',1)[0].decode(),v,sec);i+=1+aux
 def rels(self,sec):
  s=self.sections[sec];return[struct.unpack_from('<IIH',self.data,s[5]+10*i)for i in range(s[7])]
class Bundle:
 def __init__(self,objects):
  self.objects=objects;self.definitions={}
  for oi,o in enumerate(objects):
   for n,v,s in o.symbols.values():
    if s>0:self.definitions.setdefault(n,(oi,v,s))
 def select(self,prefix):
  found=[(n,v)for n,v in self.definitions.items()if n.startswith(prefix)];assert len(found)==1,found
  n,(oi,value,sec)=found[0];pending=[(oi,sec)];used=set()
  # Also make the genuine free accessor available for pointer-identity checks.
  pending +=[(o,s)for n,(o,v,s)in self.definitions.items()if n.startswith('?JobManager@CgsSystem')]
  while pending:
   key=pending.pop()
   if key in used:continue
   used.add(key);o=self.objects[key[0]]
   for at,ix,typ in o.rels(key[1]):
    if typ==0x12:continue
    name,v,s=o.symbols[ix]
    if s>0:pending.append((key[0],s))
    elif name in self.definitions:
     oo,_,ss=self.definitions[name];pending.append((oo,ss))
  self.bases={};address=0x83800000
  for key in sorted(used):
   self.bases[key]=address;size=self.objects[key[0]].sections[key[1]][3];address+=(max(size,1)+0xffff)&~0xffff
  assert address<0x84400000,hex(address)
  self.images={};self.unresolved={};external={}
  for key in sorted(used):
   o=self.objects[key[0]];s=o.sections[key[1]];body=bytearray(o.data[s[4]:s[4]+s[3]])if s[4]else bytearray(s[3])
   for at,ix,typ in o.rels(key[1]):
    if typ==0x12:continue
    name,v,sec=o.symbols[ix];crt=re.fullmatch(r'__(save|rest)gprlr_(\d+)',name)
    if sec>0:target=self.bases[(key[0],sec)]+v
    elif name in self.definitions:
     oo,v,ss=self.definitions[name];target=self.bases[(oo,ss)]+v
    elif crt:target=(0x82c08eb0 if crt[1]=='save'else 0x82c08f00)+4*(int(crt[2])-14)
    else:
     if name not in external:external[name]=0x84800000+16*len(external);self.unresolved[external[name]]=name
     target=external[name]
    w=struct.unpack_from('>I',body,at)[0]
    if typ in (0x10,0x11):
     assert w&0xffff==0;imm=((target+0x8000)>>16)if typ==0x10 else target;w=(w&0xffff0000)|(imm&0xffff)
    elif typ==6:w=(w&0xfc000003)|((target-self.bases[key]-at)&0x3fffffc)
    elif typ==2:w=(w+target)&MASK
    else:raise AssertionError((typ,name))
    struct.pack_into('>I',body,at,w)
   self.images[self.bases[key]]=body
  self.entry=self.bases[(oi,sec)]+value if False else self.addr(prefix)
 def addr(self,prefix):
  matches=[self.bases[(o,s)]+v for n,(o,v,s)in self.definitions.items()if n.startswith(prefix)and(o,s)in self.bases]
  assert len(matches)==1,(prefix,matches);return matches[0]
 def ext(self,prefix):
  matches=[a for a,n in self.unresolved.items()if n.startswith(prefix)];assert len(matches)==1,(prefix,matches);return matches[0]
def words(c,a,v):c.mem.wr(a,struct.pack('>'+'I'*len(v),*(x&MASK for x in v)))
def get(c,a,n):return list(struct.unpack('>'+'I'*n,c.mem.rd(a,4*n)))
def run(bundle,compiled,index):
 mem=F.M.V.Mem();mem.add(STACK,0x20000);mem.add(AUX,0x10000);mem.add(0x84800000,0x10000);mem.add(0x82f30000,0x1d8000)
 if compiled:
  for a,b in bundle.images.items():mem.add(a,len(b),b)
 c=F.AtomicCPU(mem);c.setg(1,STACK+0x18000);c.msr=0x8000;c.reservation=None;c.failures=0;c.atomic_trace=[];c.fence_trace=[];c.max_steps=100000
 mgr=bundle.addr('?mJobManager@')if compiled else 0x830ea650
 alloc=bundle.addr('?mJobManagerAllocator@')if compiled else 0x82f34160
 buf=bundle.addr('?macJobManagerBuffer@')if compiled else 0x83086388
 msgcell=bundle.ext('?gpcMessageBuffer@')if compiled else 0x82f32260
 capcell=bundle.ext('?giMessageBufferSize@')if compiled else 0x82f32264
 cap=(8,64,256)[index%3];words(c,msgcell,[MESSAGE]);words(c,capcell,[cap]);mem.wr(MESSAGE,b'X'*256);mem.wr(mgr,b'J'*0x30);mem.wr(alloc,b'A'*0x524)
 words(c,VTABLE,[0,0x85001000]);mem.add(0x85000000,0x2000)
 if not compiled:words(c,0x82f33fcc,[F.M.V.x360rd.u32(0x82f33fcc)])
 trace=[];param_ptr=[];thread_ctor=[0];overrides=index%4
 defaults=[0,(0,0x10000,0xffffffff,0x12340)[(index//4)%4],0x100+index,0x80+index]
 def role(a):return{mgr:'manager',alloc:'allocator',buf:'buffer'}.get(a,a)
 def hook(prefix,native,fn):
  at=bundle.ext(prefix)if compiled else native;c.hooks[at]=fn
 def construct(cpu):trace.append(['construct',role(cpu.g(3)&MASK),role(cpu.g(4)&MASK),cpu.g(5)&MASK])
 def install(cpu):trace.append(['install',role(cpu.g(3)&MASK)])
 def profiling(cpu):trace.append(['profiling',role(cpu.g(3)&MASK),cpu.g(4)&MASK]);mem.wr(mgr+0x28,bytes([cpu.g(4)&255]))
 def initialize(cpu):trace.append(['initialize',role(cpu.g(3)&MASK),cpu.g(4)&MASK,cpu.g(5)&MASK]);words(cpu,mgr,[AUX+0x8000]);mem.wr(mgr+0x29,b'\1')
 def thread_default(cpu):thread_ctor[0]+=1;trace.append(['thread_defaults',thread_ctor[0]]);words(cpu,cpu.g(3)&MASK,defaults)
 def addthread(cpu):
  result,manager,param=[cpu.g(i)&MASK for i in (3,4,5)];v=get(cpu,param,6);param_ptr.append(param)
  trace.append(['worker',role(manager),v[:5],F.cstring(cpu,v[5]),param==param_ptr[0]])
  words(cpu,result,[AUX+0x8000,len(param_ptr)])
 def begin(cpu):trace.append(['begin'])
 def fire(cpu):trace.append(['assert',F.cstring(cpu,cpu.g(3)&MASK),F.cstring(cpu,cpu.g(4)&MASK),cpu.g(5)&MASK])
 def end(cpu):trace.append(['end'])
 def strctor(cpu):
  obj,dest,size=[cpu.g(i)&MASK for i in (3,4,5)];words(cpu,obj,[VTABLE,0,dest,size]);mem.wr(dest,b'\0')
 def append(cpu):
  obj=cpu.g(3)&MASK;dest,size=get(cpu,obj+8,2);text=F.cstring(cpu,cpu.g(4)&MASK).encode();before=F.cstring(cpu,dest).encode();data=before+text[:max(0,size-1-len(before))];mem.wr(dest,data+b'\0');cpu.setg(3,obj)
 def fopen(cpu):trace.append(['fopen',F.cstring(cpu,cpu.g(3)&MASK),F.cstring(cpu,cpu.g(4)&MASK)]);raise Stop()
 hook('?Construct@HeapMallocCoreAllocator@',0x82866870,construct);hook('?SetAllocator@Jobs@',0x82bc9830,install);hook('?SetProfiling@JobScheduler@',0x82bc9ae8,profiling);hook('?Initialize@JobScheduler@',0x82bcc390,initialize);hook('?AddThread@JobScheduler@',0x82bca5f8,addthread);hook('??0ThreadParameters@',0x82b42d58,thread_default)
 hook('?BeginAssert@',0x82817548,begin);hook('?FireAssert@',0x82820810,fire);hook('?EndAssert@',0x82817558,end);hook('fopen',0x82c0acf8,fopen)
 if compiled:
  c.hooks[bundle.ext('??0StrStream@')]=strctor
 else:
  c.hooks[0x82815e58]=lambda cpu:words(cpu,(cpu.g(3)&MASK)+4,[0])
  c.hooks[F.M.V.x360rd.u32(0x82000d0c)]=append
 c.hooks[0x85001000]=append
 c.setg(3,0);c.setg(4,AUX+0x6000);c.setg(5,AUX+0x7000 if overrides&1 else 0);c.setg(6,AUX+0x7100 if overrides&2 else 0)
 try:c.run(bundle.entry if compiled else 0x828e05a0)
 except Stop:pass
 else:raise AssertionError('startup did not reach fopen')
 assert thread_ctor[0]==1
 assert len(param_ptr)==3 and len(set(param_ptr))==1
 if compiled:
  c.run(bundle.addr('?JobManager@CgsSystem'));assert(c.g(3)&MASK)==mgr
 return{'calls':trace,'message':mem.rd(MESSAGE,256).hex(),'scheduler':mem.rd(mgr,0x30).hex(),'allocator':mem.rd(alloc,0x524).hex()}
def main():
 r=json.loads((OUT/'native-provider/compile-results.json').read_text());rows={Path(x['source']).name:x for x in r['results']};objects=[Obj(rows[n]['object'])for n in('CgsHardwareInitPS3.cpp','job_thread_parameters.cpp')];bundle=Bundle(objects);bundle.select('?InitializeHardware@HardwareInit@');errors=[];count=0
 for i in range(128):
  pair=[run(bundle,c,i)for c in(False,True)];count+=1
  if pair[0]!=pair[1]:errors.append({'index':i,'original':pair[0],'compiled':pair[1]});break
 result={'cases':count,'mismatches':errors,'scope':__doc__,'objects':{str(o.path):hashlib.sha256(o.path.read_bytes()).hexdigest()for o in objects},'controlled_leaves':['HeapMallocCoreAllocator::Construct','EA::Jobs::SetAllocator','JobScheduler SetProfiling/Initialize/AddThread','EA::Thread::ThreadParameters default values','assert mutex/formatter/fire','fopen stop boundary'],'actual_code':'Both original and compiled complete startup phase through first fopen; compiled real JobThreadParameters ctor and free JobManager accessor execute.'}
 (OUT/'startup-phase-comparison.json').write_text(json.dumps(result,indent=2)+'\n')
 print('Startup phase',count,'cases;',len(errors),'mismatches',flush=True)
 if errors:print(json.dumps(errors[0],indent=2)[:8000])
 raise SystemExit(bool(errors))
if __name__=='__main__':main()
