from pathlib import Path
import sys,json,hashlib
P=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(P/'scratch/x360_sdk'));import native_atomic_fixture as F
D=Path('D:/Reverse/BP-X360-Experimental-Artifacts/bootlegal-final-caller');j=json.loads((D/'compile-results.json').read_text());obj=Path(next(x for x in j['results'] if x['source'].endswith('BrnBootLegal.cpp'))['object']);sym='?Update@BootLegal@BrnGui@@';o=F.Coff(obj,sym);b=F.Bundle([o]);b.select(sym)
for i in range(32):
 outs=[];loaded=i%2
 for native in (False,True):
  c=F.cpu(b);base=0x51000000;queue=0x52000000;iface=0x53000000;cache=0x54000000;event=0x55000000
  for a,sz in [(base,0x4000),(queue,0x1000),(iface,0x1000),(cache,0x20000),(event,0x1000)]:c.mem.add(a,sz)
  for a,v in [(base+0x18,queue),(base+0x1c,iface),(base+0x38,0),(base+0x48,0),(event,cache)]:F.put(c,a,v)
  trace=[]
  def first(x):F.put(x,x.g(4),event);x.setg(3,64)
  def nextfn(x):F.put(x,x.g(5),0);x.setg(3,-1)
  def clear(x):trace.append(('clear',x.g(3)&0xffffffff))
  def resources(x):trace.append(('resources',x.g(3)&0xffffffff,x.g(5),x.mem.rd(x.g(4)&0xffffffff,8)));x.setg(3,loaded)
  def resource(x):trace.append(('resource',x.g(3)&0xffffffff,x.mem.rd(x.g(4)&0xffffffff,8)));x.setg(3,1)
  for a,f in [(0x82360fd8,first),(0x8236a5e0,nextfn),(0x8235f470,clear),(0x824feb58,resources),(0x824feb50,resource)]:c.hooks[a]=f
  for n,v,sec in o.symbols.values():
   if (0,sec) not in b.bases:continue
   a=b.bases[(0,sec)]+v
   if '?GetFirstEvent@?$VariableEventQueue@$0EIAA@' in n:c.hooks[a]=first
   elif '?GetNextEvent@?$VariableEventQueue@$0EIAA@' in n:c.hooks[a]=nextfn
   elif '?Clear@?$VariableEventQueue@$0EIAA@' in n:c.hooks[a]=clear
  for a,n in b.unresolved.items():
   if '?EnsureResourcesAreLoaded@GuiCache@' in n:c.hooks[a]=resources
   elif '?EnsureResourceIsLoaded@GuiCache@' in n:c.hooks[a]=resource
  c.setg(3,base)
  try:c.run(b.entry if native else 0x82477270)
  except Exception:print('fail',i,native,hex(c.last_pc));raise
  outs.append((trace,c.mem.rd(base+0x38,4),c.mem.rd(base+0x48,4)))
 assert outs[0]==outs[1],(i,outs)
out={'passed':True,'cases':32,'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'Actual whole BootLegal::Update stage0 receiving cache event vs ARTIST 82477270, loaded/unloaded resource outcomes; compares raw cache receiver, resource tuples, queue clear and state writes. Queue/cache services controlled, modeled CPU.'};(D/'boot-stage0-comparison.json').write_text(json.dumps(out,indent=2)+'\n');print(out)
