"""Bounded native audio clock, factory and command-pump comparisons.

Real compiled System bodies execute; allocator/mutex/timer/command services are
controlled leaves. Timebase samples are controlled at the actual mftb instruction.
No real physical allocation, audio playback or kernel concurrency is exercised.
"""
from pathlib import Path
import hashlib,json,random,struct,sys
OUT=Path(__file__).resolve().parent
sys.path.insert(0,'D:/Reverse/BP-X360-Experimental-Artifacts/job-provider-526adc')
from validate_startup_phase import Obj,Bundle,F
MASK=0xffffffff;SELF=0x50000000;STACK=0x40000000;AUX=0x60000000;ALLOC=AUX+0x1000;VT=AUX+0x2000;VOICE=AUX+0x3000;RING=AUX+0x5000
class CPU(F.AtomicCPU):
 def step(self,pc,word,nxt,depth):
  if word&~(31<<21)==0x7c6c42e6&~(31<<21):
   assert self.clock_index<len(self.clocks);self.setg((word>>21)&31,self.clocks[self.clock_index]);self.clock_index+=1;return nxt
  return super().step(pc,word,nxt,depth)
def words(c,a,v):c.mem.wr(a,struct.pack('>'+'I'*len(v),*(x&MASK for x in v)))
def u32(c,a):return struct.unpack('>I',c.mem.rd(a,4))[0]
def make(bundle,compiled):
 m=F.M.V.Mem();m.add(STACK,0x20000);m.add(SELF,0x2000);m.add(AUX,0x10000);m.add(0x83270000,0x10000);m.add(0x84800000,0x10000);m.add(0x85000000,0x2000)
 if compiled:
  for a,b in bundle.images.items():m.add(a,len(b),b)
 c=CPU(m);c.setg(1,STACK+0x18000);c.max_steps=100000;c.clock_index=0;c.clocks=[];c.atomic_trace=[];c.fence_trace=[];c.msr=0x8000;c.reservation=None;c.failures=0
 if compiled:
  for a,n in bundle.unresolved.items():
   def reject(cpu,name=n):raise AssertionError(('unexpected native boundary',name))
   c.hooks[a]=reject
 return c
def bind(c,bundle,compiled,prefix,original,fn):
 at=bundle.ext(prefix)if compiled else original;c.hooks[at]=fn
def factory(bundle,compiled,index):
 c=make(bundle,compiled);m=c.mem;rng=random.Random(938800+index);m.wr(SELF,rng.randbytes(0x1174));m.wr(VOICE,rng.randbytes(16));words(c,ALLOC,[VT]);words(c,VT,[0,0x85001000,0x85001020,0x85001010]);trace=[];nalloc=[0]
 global_at=bundle.addr('off_83271928')if compiled else 0x83271928;table=bundle.addr('skSystemStackAllocator')if compiled else 0x83271930
 words(c,global_at,[0x5ffff00d]);m.wr(table,b'T'*16);fail=index%4;request=(0,1,64,0x40000,0xffffffff)[(index//4)%5];param=[]
 def role(a):
  if SELF<=a<SELF+0x1174:return['self',a-SELF]
  return{ALLOC:'allocator',VOICE:'voice_node',RING:'ring',0:'null',0x5ffff00d:'old_singleton'}.get(a,a)
 def allocate(cpu):
  n=nalloc[0];nalloc[0]+=1;args=[cpu.g(i)&MASK for i in range(3,9)];trace.append(['alloc',role(args[0]),args[1],F.cstring(cpu,args[2]),*args[3:],role(u32(cpu,global_at))])
  cpu.setg(3,0 if fail==n else(SELF,VOICE,RING)[n])
 def free(cpu):trace.append(['free',role(cpu.g(3)&MASK),role(cpu.g(4)&MASK),cpu.g(5)&MASK,role(u32(cpu,global_at))])
 def params(cpu):param.append(cpu.g(3)&MASK);trace.append(['mutex_params',cpu.g(4)&MASK,cpu.g(5)&MASK])
 def mutex(cpu):
  at=cpu.g(3)&MASK;trace.append(['mutex_ctor',role(at),cpu.g(4)&MASK,cpu.g(5)&MASK,role(u32(cpu,global_at))]);words(cpu,at,[0xabcdef01]);cpu.setg(3,at)
 def init(cpu):trace.append(['mutex_init',role(cpu.g(3)&MASK),(cpu.g(4)&MASK)==param[0]]);cpu.setg(3,1)
 def timer(cpu):
  at=cpu.g(3)&MASK;trace.append(['timer_ctor',role(at)]);m.wr(at,b'\0'*56);m.wr(at+56,struct.pack('>f',-1.0));words(cpu,at+68,[0]);cpu.setg(3,at)
 def memset(cpu):
  dst,val,n=[cpu.g(i)&MASK for i in(3,4,5)];m.wr(dst,bytes([val&255])*n);cpu.setg(3,dst)
 c.hooks[0x85001000]=allocate;c.hooks[0x85001010]=free
 bind(c,bundle,compiled,'??0MutexParameters@',0x82b42710,params);bind(c,bundle,compiled,'??0Mutex@',0x82b43690,mutex);bind(c,bundle,compiled,'?Init@Mutex@',0x82b42770,init);bind(c,bundle,compiled,'?TimerManager_ctor@',0x82b6bed0,timer)
 if compiled:
  for a,n in bundle.unresolved.items():
   if n in('memset','XMemSet'):c.hooks[a]=memset
 else:c.hooks[0x82926fd0]=memset
 c.setg(3,ALLOC);c.setg(4,request);c.run(bundle.entry if compiled else 0x82b6f420)
 data=bytearray(m.rd(SELF,0x1174))
 if fail!=0:struct.pack_into('>I',data,0,0x83271930)
 return{'calls':trace,'return':role(c.g(3)&MASK),'system':data.hex(),'stack_table':m.rd(table,16).hex(),'singleton':role(u32(c,global_at)),'voice_node':m.rd(VOICE,16).hex()}
def pump(bundle,compiled,index):
 c=make(bundle,compiled);m=c.mem;rng=random.Random(64558+index);m.wr(SELF,rng.randbytes(0x1108));trace=[];locked=[False]
 words(c,SELF+0x40,[0x85001100,0x85001110]);words(c,SELF+0x5c,[0]);words(c,SELF+0x10a8,[0]);count=index%4;stride=(8,16)[(index//4)%2]
 words(c,SELF+0x20,[RING]);words(c,SELF+0x10b8,[count*stride,(0,0x1000)[index%2]]);words(c,SELF+0x10e4,[0xffffffff if index%3==0 else index]);m.wr(SELF+0x10c0,struct.pack('>f',0.01))
 for i in range(count):words(c,RING+i*stride,[0x85001120,i])
 clocks=[0,1,0xffffffff,0x100000000,0xffffffffffffffff,0x80000000,0x100000001,0x7fffffffffffffff]
 if index%2:clocks=[rng.getrandbits(64)for _ in range(8)]
 c.clocks=clocks
 def lock(cpu):assert not locked[0];locked[0]=True;trace.append(['lock'])
 def unlock(cpu):assert locked[0];locked[0]=False;trace.append(['unlock'])
 def timers(cpu):trace.append(['timers',cpu.g(3)&MASK==SELF+0x60,cpu.g(4)&MASK,locked[0]])
 def defrag(cpu):trace.append(['defrag',cpu.g(3)&MASK==SELF+0x60,locked[0]])
 def command(cpu):
  offset=(cpu.g(3)&MASK)-RING;trace.append(['command',offset,locked[0]])
  if index%3==0 and offset==0:words(cpu,SELF+0x10b8,[u32(cpu,SELF+0x10b8)+8])
  cpu.setg(3,stride)
 c.hooks[0x85001100]=lock;c.hooks[0x85001110]=unlock;c.hooks[0x85001120]=command
 bind(c,bundle,compiled,'?ExecuteTimers@',0x82b6bf18,timers);bind(c,bundle,compiled,'?Defragment@TimerManager@',0x82b6ec08,defrag)
 c.setg(3,SELF);c.run(bundle.entry if compiled else 0x82b6f6d0);assert c.clock_index==8 and not locked[0]
 return{'calls':trace,'system':m.rd(SELF,0x1108).hex(),'clock_reads':c.clock_index}
def counter(bundle,compiled,value):
 c=make(bundle,compiled);c.clocks=[value];c.run(bundle.entry if compiled else 0x82b64558);assert c.clock_index==1;return c.g(3)&MASK

def main():
 report=json.loads((OUT/'native-final/native/compile-results.json').read_text());row=report['results'][0];o=Obj(row['object']);bundle=Bundle([o]);errors=[];totals=[]
 for kind,prefix,runner,count in [('factory','?CreateInstance@System@',factory,128),('pump','?ExecuteCommands@System@',pump,128),('counter','?GetCpuCycle@',counter,512)]:
  bundle.select(prefix)
  for i in range(count):
   arg=i if kind!='counter'else([0,1,0xffffffff,0x100000000,0x100000001,0x7fffffffffffffff,0x8000000000000000,0xffffffffffffffff][i%8]if i<64 else random.Random(9388+i).getrandbits(64))
   pair=[runner(bundle,c,arg)for c in(False,True)]
   if pair[0]!=pair[1]:errors.append({'function':kind,'case':i,'input':arg,'original':pair[0],'compiled':pair[1]});break
  totals.append({'function':kind,'cases':i+1});print(kind,i+1,'cases;',len(errors),'mismatches',flush=True)
  if errors:break
 result={'functions':totals,'cases':sum(x['cases']for x in totals),'mismatches':errors,'scope':__doc__,'object':row['object'],'object_sha256':hashlib.sha256(Path(row['object']).read_bytes()).hexdigest(),'clock_limit':'Original function has2 instructions; compiler intrinsic/u32 projection has3. Return lowword and one timebase read are compared, not unused upper register bits.','controlled_leaves':['allocator virtualAlloc/Free','EAThread mutex ctor/Init','TimerManager constructor/execute/defragment','ring callbacks','XMemSet/memset','actual mftb instruction input']}
 (OUT/'native-comparison.json').write_text(json.dumps(result,indent=2)+'\n')
 if errors:
  e=errors[0];print(e['function'],e['case']);print(str(e)[:7000])
 raise SystemExit(bool(errors))
if __name__=='__main__':main()
