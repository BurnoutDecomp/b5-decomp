from pathlib import Path
import sys,json,hashlib,struct
P=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(P/'scratch/x360_sdk'));import native_atomic_fixture as F
D=Path('D:/Reverse/BP-X360-Experimental-Artifacts/gui-event-provider-final');j=json.loads((D/'compile-results.json').read_text());obj=Path(next(x for x in j['results'] if x['source'].endswith('BrnBootLegal.cpp'))['object']);sym='?Update@BootLegal@BrnGui@@';o=F.Coff(obj,sym);b=F.Bundle([o]);b.select(sym);h=0x123456789abcdef0
for i in range(96):
 eventid=[189,190,567][i%3];match=(i//3)%2;leave=(i//6)%2;outs=[]
 for native in (False,True):
  c=F.cpu(b);base=0x51000000;queue=0x52000000;iface=0x53000000;cache=0x54000000;event=0x55000000
  for a,sz in [(base,0x4000),(queue,0x1000),(iface,0x1000),(cache,0x20000),(event,0x1000)]:c.mem.add(a,sz)
  for a,v in [(base+0x18,queue),(base+0x1c,iface),(base+0x38,cache),(base+0x48,10),(cache+4,0x418a0000),(event+8,leave)]:F.put(c,a,v)
  c.mem.wr(event,(h if match else h^0x100000000).to_bytes(8,'big'));c.mem.wr(base+0x44,b'\0');c.mem.wr(base+0x50,b'\1');trace=[]
  def first(x):F.put(x,x.g(4),event);x.setg(3,eventid)
  def nextfn(x):F.put(x,x.g(5),0);x.setg(3,-1)
  def clear(x):trace.append(('clear',x.g(3)&0xffffffff))
  def hashfn(x):trace.append(('hash',F.cstring(x,x.g(3)&0xffffffff)));x.setg(3,h)
  def gettime(x):x.setf(1,17.25)
  for a,f in [(0x82360fd8,first),(0x8236a5e0,nextfn),(0x8235f470,clear),(0x82815a20,hashfn),(0x8240f0b0,gettime)]:c.hooks[a]=f
  for n,v,sec in o.symbols.values():
   if (0,sec) not in b.bases:continue
   a=b.bases[(0,sec)]+v
   if '?GetFirstEvent@?$VariableEventQueue@$0EIAA@' in n:c.hooks[a]=first
   elif '?GetNextEvent@?$VariableEventQueue@$0EIAA@' in n:c.hooks[a]=nextfn
   elif '?Clear@?$VariableEventQueue@$0EIAA@' in n:c.hooks[a]=clear
  for a,n in b.unresolved.items():
   if '?CgsIDCompress@@' in n:c.hooks[a]=hashfn
   elif '?GetTime@GuiCache@' in n:c.hooks[a]=gettime
  c.setg(3,base)
  try:c.run(b.entry if native else 0x82477270)
  except Exception:print('fail',i,native,hex(c.last_pc));raise
  outs.append((trace,c.mem.rd(base+0x40,0x18)))
 assert outs[0]==outs[1],(i,outs)
out={'passed':True,'cases':96,'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'Actual BootLegal Update overlay-opened190, overlay-complete189 and resource-ready567 against ARTIST with matching/nonmatching full64-bit IDs and leave-method variants; stage10 default, state bytes and queue traces. Hash, queue and native time getter controlled; modeled CPU.'};(D/'boot-event-comparison.json').write_text(json.dumps(out,indent=2)+'\n');print(out)
