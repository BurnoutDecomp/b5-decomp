import sys,struct,json,hashlib
from pathlib import Path
sys.path.insert(0,r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\scratch\x360_sdk');import native_atomic_fixture as F
root=Path(r'D:\Reverse\BP-X360-Experimental-Artifacts\native-job-entries-root\native');report=json.loads((root/'compile-results.json').read_text());out=Path(r'D:\Reverse\BP-X360-Experimental-Artifacts\parallel\job_entries_author_terra');A=F.M.S.AUX
specs=[('Relocator.cpp','?RelocatorEntry@@',0x82AD28C0,0x82AD2BF8,0x83248C00,0xC80,55,'Relocator'),('RadixSort.cpp','?RadixSortEntry@@',0x82AD2020,0x82AD2818,0x83248B98,1,59,'RadixSort'),('ContactGenerator.cpp','?ContactGeneratorEntry@@',0x82920F10,0x829267E0,0x831BBF80,0x10300,55,'ContactGenerator'),('PolygonSoupTester.cpp','?PolygonSoupTesterEntry@@',0x829157B8,0x82915930,0x83123940,0x19040,55,'PolygonSoupTester')]
class TraceCPU(F.AtomicCPU):
 def step(self,pc,word,nxt,depth):
  if word>>26==31 and ((word>>1)&1023)==457:
   dst=(word>>21)&31;ra=(word>>16)&31;rb=(word>>11)&31;divisor=self.g(rb)&0xffffffffffffffff;assert divisor;self.setg(dst,(self.g(ra)&0xffffffffffffffff)//divisor);return nxt
  if word==0x7d8000a6:self.setg(12,0);return nxt
  if word==0x7d800124:self.breaks+=1;return nxt
  return super().step(pc,word,nxt,depth)
def u32(c,a):return int.from_bytes(c.mem.rd(a,4),'big')
def readstr(c,a):return F.cstring(c,a&0xffffffff)
results=[]
for name,prefix,orig,callee,base,stride,line,tag in specs:
 row=next(x for x in report['results'] if x['source'].endswith('/'+name));o=F.Coff(Path(row['object']),prefix);compiled_base=None
 for idx in [0,1,2,3,4,5,6,-1]:
  pair=[];tid=0xF8000088+4*idx;payload=A+0x7000
  for native in [False,True]:
   c=F.cpu(o);c.__class__=TraceCPU;c.breaks=0;c.mem.add(0x83015000,0x3000);c.mem.wr(A,bytes(0x10000));F.put(c,A+0x4104,A+0x4200);trace=[];calls=[0]
   def thread(z):calls[0]+=1;z.setg(3,tid);z.setg(4,0x12345678)
   def ctor(z):
    this=z.g(3)&0xffffffff;ptr=z.g(4)&0xffffffff;assert (z.g(5)&0xffffffff)==8192;assert (ptr in [a for a,n in o.unresolved.items() if n.startswith('?byte_83015F20@')]) if native else ptr==0x83015F20;F.put(z,this,A+0x4100);F.put(z,this+4,0);F.put(z,this+8,ptr);F.put(z,this+12,8192);z.mem.wr(ptr,b'\0');z.setg(3,this)
   def append(z):
    this=z.g(3)&0xffffffff;ptr=u32(z,this+8);assert (ptr in [a for a,n in o.unresolved.items() if n.startswith('?byte_83015F20@')]) if native else ptr==0x83015F20;s=readstr(z,ptr)+readstr(z,z.g(4));z.mem.wr(ptr,s.encode()+b'\0');z.setg(3,this)
   def numeric(z):
    this=z.g(3)&0xffffffff;ptr=u32(z,this+8);s=readstr(z,ptr)+str(z.g(4)&0xffffffffffffffff);z.mem.wr(ptr,s.encode()+b'\0');z.setg(3,this)
   def clear(z):F.put(z,(z.g(3)&0xffffffff)+4,0)
   def getbuf(z):z.setg(3,u32(z,(z.g(3)&0xffffffff)+8))
   def assertion(z):trace.append(('assert',readstr(z,z.g(3)),z.g(4)&0xffffffff,z.g(5)&0xffffffff,readstr(z,z.g(6))))
   def execute(z):trace.append(('execute',z.g(3)&0xffffffff,z.g(4)&0xffffffff,(z.g(5)&0xffffffff) if tag=='Relocator' else None))
   c.hooks.update({0x82B42560:thread,0x829158F8:thread,0x82815E58:clear,0x821F0158:ctor,0x821F01A8:append,0x821F0E48:getbuf,0x82815E80:append,0x82203EE8:numeric,0x82815928:assertion,callee:execute,A+0x4200:append})
   if native:
    for a,n in o.unresolved.items():
     if n.startswith('?GetThreadId@Thread@EA@@') or n.startswith('?GetJobThreadId@Jobs@EA@@'):c.hooks[a]=thread
     elif n.startswith('??0StrStream@CgsDev@@'):c.hooks[a]=ctor
     elif n.startswith('??6StrStreamBase@CgsDev@@'):c.hooks[a]=numeric
     elif n.startswith('?PrintStringed@Assert@CgsDev@@'):c.hooks[a]=assertion
     elif n.startswith('?Execute@'):c.hooks[a]=execute
     elif n.startswith('??1StrStream@CgsDev@@'):c.hooks[a]=lambda z:None
   c.setg(3,0x111);c.setg(4,payload);c.setg(5,0x222);c.setg(6,0x333);c.run(o.entry if native else orig);pair.append((trace,calls[0],c.breaks))
  original,compiled=pair
  if idx==0:compiled_base=compiled[0][-1][1]
  ot,ct=original[0],compiled[0];oe,ce=ot[-1],ct[-1]
  if oe[1] != ((base+idx*stride)&0xffffffff) or ce[1] != ((compiled_base+idx*stride)&0xffffffff) or oe[2:]!=ce[2:] or ot[:-1]!=ct[:-1] or original[1:]!=compiled[1:]:raise AssertionError((name,idx,pair,hex(compiled_base)))
  invalid=idx<0 or idx>=6;expected_asserts=2 if invalid else 0;assert len(ot)-1==expected_asserts and original[1]==(2 if invalid else 1) and original[2]==(2 if invalid else 0),(name,idx,pair)
  results.append({'entry':tag,'index':idx,'original_receiver':hex(oe[1]),'compiled_receiver':hex(ce[1]),'payload':hex(oe[2]),'jobid':oe[3],'assertions':ot[:-1],'thread_calls':original[1],'msr_writes':original[2]})
 print(tag,'pass',8,flush=True)
res={'scope':'four actual original and compiled entry bodies; six real native worker indices and two invalid indices (upper bound and below the thread-ID base), controlled thread, stream, worker and assert services; compares all service traces/argument metadata, payload and relative real worker stride; r5 compared for Relocator only; original global bases independently pinned; PPC emulator counts MSR writes but does not establish hardware behavior','cases':len(results),'results':results,'objects':{x['source']:hashlib.sha256(Path(x['object']).read_bytes()).hexdigest() for x in report['results'] if any(x['source'].endswith('/'+z[0]) for z in specs)}};(out/'entry-comparison.json').write_text(json.dumps(res,indent=2)+'\n');print('all cases',len(results),'pass')


