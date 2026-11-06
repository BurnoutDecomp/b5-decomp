from pathlib import Path
import sys,json,struct,random,hashlib
P=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(P/'scratch/x360_sdk'));import native_atomic_fixture as F
D=Path('D:/Reverse/BP-X360-Experimental-Artifacts/bootlegal-terra-final');j=json.loads((D/'compile-results.json').read_text());obj=Path(next(x for x in j['results'] if x['source'].endswith('BrnBootLegal.cpp'))['object']);sym='?Update@BootLegal@BrnGui@@';o=F.Coff(obj,sym);b=F.Bundle([o]);b.select(sym)
for i in range(48):
 stage=[2,3,4][i%3];hd=(i>>2)&1;sku=(i>>3)&1;flag=(i>>4)&1;wait=(i>>5)&1;outs=[]
 for native in (False,True):
  c=F.cpu(b);base=0x51000000;queue=0x52000000;iface=0x53000000;cache=0x54000000
  for addr,size in [(base,0x4000),(queue,0x1000),(iface,0x1000),(cache,0x20000)]:c.mem.add(addr,size)
  F.put(c,base+0x18,queue);F.put(c,base+0x1c,iface);F.put(c,base+0x38,cache);F.put(c,base+0x48,stage);F.put(c,base+0x4c,struct.unpack('>I',struct.pack('>f',10.))[0]);c.mem.wr(base+0x44,bytes([wait]));c.mem.wr(base+0x50,bytes([flag]));c.mem.wr(cache+0x4b49,bytes([hd]));F.put(c,cache+0xa7f4,sku);c.mem.wr(cache+0x12f0a,bytes([flag,0]));trace=[]
  def first(x):F.put(x,x.g(4),0);x.setg(3,-1)
  def clear(x):trace.append(('clear',x.g(3)&0xffffffff))
  def gettime(x):trace.append(('time',x.g(3)&0xffffffff));x.setf(1,17.25)
  def ready(x):trace.append(('ready',x.g(3)&0xffffffff,x.g(4)));x.setg(3,1)
  def view(x):trace.append(('view',x.g(3)&0xffffffff,F.cstring(x,x.g(4)&0xffffffff),F.cstring(x,x.g(5)&0xffffffff),x.g(6)))
  def output(x):x.setg(3,(x.g(3)+12)&0xffffffff)
  def add(x):trace.append(('add',x.g(3)&0xffffffff,x.g(5),x.g(6),x.mem.rd(x.g(4)&0xffffffff,x.g(6))))
  for addr,fn in [(0x82360fd8,first),(0x8235f470,clear),(0x8240f0b0,gettime),(0x824ee7a8,ready),(0x8285bb48,view),(0x82435188,add)]:c.hooks[addr]=fn
  for n,v,sec in o.symbols.values():
   if (0,sec) not in b.bases:continue
   addr=b.bases[(0,sec)]+v
   if '?GetFirstEvent@?$VariableEventQueue@$0EIAA@' in n:c.hooks[addr]=first
   elif '?Clear@?$VariableEventQueue@$0EIAA@' in n:c.hooks[addr]=clear
   elif '?AddEvent@?$VariableEventQueue@$0BAAAA@' in n:c.hooks[addr]=add
  for addr,n in b.unresolved.items():
   if '?GetTime@GuiCache@' in n:c.hooks[addr]=gettime
   elif '?AreAllAptComponentsInitialised@GuiCache@' in n:c.hooks[addr]=ready
   elif '?AddOutputAptViewState@GuiComponent@' in n:c.hooks[addr]=view
   elif '?GetOutputEventQueue@StateInterface@' in n:c.hooks[addr]=output
  c.setg(3,base)
  try:c.run(b.entry if native else 0x82477270)
  except Exception:print('failed',i,'native',native,'pc',hex(c.last_pc));raise
  outs.append((c.mem.rd(base+0x48,16),trace))
 if outs[0]!=outs[1]:print('MISMATCH',i,'stage',stage,'hd,sku,flag,wait',hd,sku,flag,wait,outs);break
else:
 out={'passed':True,'cases':48,'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'Actual whole BootLegal::Update vs ARTIST 82477270 with no queued input events, cases 2/3/4 covering HD/SKU/overlay/sign-in/wait flags and time. Compare stage and timer bytes, call receivers, component strings and controlled queue/service traces; modeled CPU, no full lifecycle or runtime claim.'};(D/'boot-stage-comparison.json').write_text(json.dumps(out,indent=2)+'\n');print(out)
