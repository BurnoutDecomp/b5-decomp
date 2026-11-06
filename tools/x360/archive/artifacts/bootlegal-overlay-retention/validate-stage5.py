from pathlib import Path
import sys,json,hashlib,struct
P=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(P/'scratch/x360_sdk'));import native_atomic_fixture as F
D=Path('D:/Reverse/BP-X360-Experimental-Artifacts/bootlegal-overlay-retention');j=json.loads((D/'compile-results.json').read_text());obj=Path(next(x for x in j['results'] if x['source'].endswith('BrnBootLegal.cpp'))['object']);sym='?Update@BootLegal@BrnGui@@';o=F.Coff(obj,sym);b=F.Bundle([o]);b.select(sym)
for i in range(32):
 outs=[]
 for native in (False,True):
  c=F.cpu(b);base=0x51000000;queue=0x52000000;iface=0x53000000
  for a,sz in [(base,0x4000),(queue,0x1000),(iface,0x1000),(0x83008000,0x1000)]:c.mem.add(a,sz)
  for a,v in [(base+0x18,queue),(base+0x1c,iface),(base+0x40,0),(base+0x48,5),(0x830082a8,0)]:F.put(c,a,v)
  trace=[]
  def first(x):F.put(x,x.g(4),0);x.setg(3,-1)
  def clear(x):trace.append(('clear',x.g(3)&0xffffffff))
  def music(x):trace.append(('music',x.g(3)&0xffffffff,x.mem.rd(x.g(4)&0xffffffff,6)))
  def prepare(x):x.mem.wr(x.g(3)&0xffffffff,bytes((k*3+5)&255 for k in range(48)));trace.append(('prepare',))
  def rsrc(x):trace.append(('resourcehash',F.cstring(x,x.g(3)&0xffffffff)));x.setg(3,0x1234abcd)
  def sound(x):trace.append(('soundhash',F.cstring(x,x.g(3)&0xffffffff)));x.setg(3,0x55667788)
  def video(x):trace.append(('video',x.g(3)&0xffffffff,x.mem.rd(x.g(4)&0xffffffff,39)))
  for a,f in [(0x82360fd8,first),(0x8235f470,clear),(0x82476c00,music),(0x82472990,prepare),(0x828d84a8,rsrc),(0x82689a50,sound),(0x82476c68,video)]:c.hooks[a]=f
  for n,v,sec in o.symbols.values():
   if (0,sec) not in b.bases:continue
   a=b.bases[(0,sec)]+v
   if '?GetFirstEvent@?$VariableEventQueue@$0EIAA@' in n:c.hooks[a]=first
   elif '?Clear@?$VariableEventQueue@$0EIAA@' in n:c.hooks[a]=clear
  for a,n in b.unresolved.items():
   if 'OutputGuiEvent@VGuiEventPlayMusicOnMenuStream' in n:c.hooks[a]=music
   elif '?Prepare@VideoDefinition@' in n:c.hooks[a]=prepare
   elif '?HashString@ID@CgsResource@@' in n:c.hooks[a]=rsrc
   elif '?MakeHash@Name@Playback@CgsSound@@' in n:c.hooks[a]=sound
   elif 'OutputGuiEvent@VGuiEventPlayVideo' in n:c.hooks[a]=video
  c.setg(3,base)
  try:c.run(b.entry if native else 0x82477270)
  except Exception:print('fail',i,native,hex(c.last_pc));raise
  outs.append((trace,c.mem.rd(base+0x48,4)))
 assert outs[0]==outs[1],(i,outs)
out={'passed':True,'cases':32,'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'Actual BootLegal::Update stage5 vs ARTIST 82477270, same input and raw receiver addresses; compares music payload, resource and sound name strings, prepared 39 defined video fields, queue trace, and stage. Prepare/hash/event/queue services controlled, modeled CPU; no movie runtime claim.'};(D/'boot-stage5-comparison.json').write_text(json.dumps(out,indent=2)+'\n');print(out)
