from pathlib import Path
import sys,json,hashlib
P=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(P/'scratch/x360_sdk'));import native_atomic_fixture as F
class SubCarryCPU(F.AtomicCPU):
 def step(self,pc,w,nxt,depth):
  xo=(w>>1)&1023
  if w>>26==31 and xo in (8,136):
   rd=(w>>21)&31;ra=(w>>16)&31;rb=(w>>11)&31;mask=(1<<64)-1
   a=self.g(ra)&mask;b=self.g(rb)&mask
   value=(b+((~a)&mask)+(1 if xo==8 else getattr(self,'xer_ca',0)))
   self.setg(rd,value&mask);self.xer_ca=int(value>>64)
   return nxt
  return super().step(pc,w,nxt,depth)

D=Path('D:/Reverse/BP-X360-Experimental-Artifacts/gui-event-provider-final');j=json.loads((D/'compile-results.json').read_text());obj=Path(next(x for x in j['results'] if x['source'].endswith('BrnBootLegal.cpp'))['object']);sym='?Update@BootLegal@BrnGui@@';o=F.Coff(obj,sym);b=F.Bundle([o]);b.select(sym)
for i in range(96):
 kind=i%3;stage=9 if kind==2 else 6;eventid=510 if kind==0 else 6 if kind==1 else None;time=[10.25,10.5,10.75][(i//3)%3];outs=[]
 for native in (False,True):
  c=F.cpu(b);c.__class__=SubCarryCPU;base=0x51000000;queue=0x52000000;iface=0x53000000;cache=0x54000000;event=0x55000000
  for a,sz in [(base,0x4000),(queue,0x1000),(iface,0x1000),(cache,0x20000),(event,0x1000)]:c.mem.add(a,sz)
  for a,v in [(base+0x18,queue),(base+0x1c,iface),(base+0x38,cache),(base+0x40,0),(base+0x48,stage),(base+0x54,0x41200000),(event+4,49)]:F.put(c,a,v)
  trace=[]
  def first(x):F.put(x,x.g(4),event if eventid is not None else 0);x.setg(3,eventid if eventid is not None else -1)
  def nextfn(x):F.put(x,x.g(5),0);x.setg(3,-1)
  def clear(x):trace.append(('clear',x.g(3)&0xffffffff))
  def prep(x):x.mem.wr(x.g(3)&0xffffffff,bytes((k*3+5)&255 for k in range(48)));trace.append(('prepare',))
  def stop(x):trace.append(('stop',x.g(3)&0xffffffff,x.mem.rd(x.g(4)&0xffffffff,39)))
  def gettime(x):trace.append(('time',x.g(3)&0xffffffff));x.setf(1,time)
  def out(x):x.setg(3,(x.g(3)+12)&0xffffffff)
  def add(x):trace.append(('add',x.g(3)&0xffffffff,x.g(5),x.g(6),x.mem.rd(x.g(4)&0xffffffff,12)))
  for a,f in [(0x82360fd8,first),(0x8236a5e0,nextfn),(0x8235f470,clear),(0x82472990,prep),(0x82476cf8,stop),(0x8240f0b0,gettime),(0x82435188,add)]:c.hooks[a]=f
  for n,v,sec in o.symbols.values():
   if (0,sec) not in b.bases:continue
   a=b.bases[(0,sec)]+v
   if '?GetFirstEvent@?$VariableEventQueue@$0EIAA@' in n:c.hooks[a]=first
   elif '?GetNextEvent@?$VariableEventQueue@$0EIAA@' in n:c.hooks[a]=nextfn
   elif '?Clear@?$VariableEventQueue@$0EIAA@' in n:c.hooks[a]=clear
   elif '?AddEvent@?$VariableEventQueue@$0BAAAA@' in n:c.hooks[a]=add
  for a,n in b.unresolved.items():
   if '?Prepare@VideoDefinition@' in n:c.hooks[a]=prep
   elif 'OutputGuiEvent@VGuiEventStopVideo' in n:c.hooks[a]=stop
   elif '?GetTime@GuiCache@' in n:c.hooks[a]=gettime
   elif '?GetOutputEventQueue@StateInterface@' in n:c.hooks[a]=out
  c.setg(3,base)
  try:c.run(b.entry if native else 0x82477270)
  except Exception:print('fail',i,native,hex(c.last_pc));raise
  outs.append((trace,c.mem.rd(base+0x40,0x18)))
 assert outs[0]==outs[1],(i,outs)
out={'passed':True,'cases':96,'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'Actual BootLegal Update stage6 (finished/stop events) and stage9 (dwell threshold) vs ARTIST 82477270; raw state bytes, queue and video payload traces compared. Queue, Prepare, output and clock services controlled; modeled CPU, other stages/events excluded.'};(D/'boot-stage6-9-comparison.json').write_text(json.dumps(out,indent=2)+'\n');print(out)
