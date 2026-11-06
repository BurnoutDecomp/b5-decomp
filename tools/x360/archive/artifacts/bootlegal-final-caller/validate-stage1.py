from pathlib import Path
import sys,json,hashlib
P=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(P/'scratch/x360_sdk'));import native_atomic_fixture as F
D=Path('D:/Reverse/BP-X360-Experimental-Artifacts/bootlegal-final-caller');j=json.loads((D/'compile-results.json').read_text());obj=Path(next(x for x in j['results'] if x['source'].endswith('BrnBootLegal.cpp'))['object']);sym='?Update@BootLegal@BrnGui@@';o=F.Coff(obj,sym);b=F.Bundle([o]);b.select(sym)
for i in range(32):
 outs=[];soft=i%2
 for native in (False,True):
  c=F.cpu(b);base=0x51000000;queue=0x52000000;iface=0x53000000;cache=0x54000000;game=0x57000000
  for addr,size in [(base,0x4000),(queue,0x1000),(iface,0x1000),(cache,0x20000),(game,0x1000),(0x83010000,0x1000)]:c.mem.add(addr,size)
  for a,v in [(base+0x18,queue),(base+0x1c,iface),(base+0x38,cache),(base+0x48,1),(0x830102d0,game)]:F.put(c,a,v)
  for off,name in [(0x58,'HDCompAnimator_mc'),(0xe4,'esrb_anim'),(0x170,'StartMessageAnimatorComponent'),(0x1fc,'BackgroundAnimatorComponent'),(0x288,'SelectionMenuAnimatorComponent')]:c.mem.wr(base+off+4,name.encode()+b'\0')
  trace=[]
  def first(x):F.put(x,x.g(4),0);x.setg(3,-1)
  def clear(x):trace.append(('queue-clear',x.g(3)&0xffffffff))
  def watchclear(x):trace.append(('watch-clear',x.g(3)&0xffffffff,x.g(4)))
  def watchappend(x):trace.append(('watch-add',x.g(3)&0xffffffff,x.g(4),F.cstring(x,x.g(5)&0xffffffff)))
  def add(x):
   size=x.g(6);rec=x.mem.rd(x.g(4)&0xffffffff,size);trace.append(('add',x.g(3)&0xffffffff,x.g(5),size,rec[:13] if rec[4:8]==b'\x00\x00\x00X' else rec[:12]))
  def out(x):x.setg(3,(x.g(3)+12)&0xffffffff)
  def apt(x):trace.append(('apt',x.g(3)&0xffffffff,F.cstring(x,x.g(4)&0xffffffff),x.g(5)))
  def h(x):trace.append(('hash',F.cstring(x,x.g(3)&0xffffffff)));x.setg(3,0x12345678)
  def music(x):trace.append(('music',x.g(3)&0xffffffff,x.mem.rd(x.g(4)&0xffffffff,6)))
  def soften(x):trace.append(('soft',x.g(3)&0xffffffff));x.setg(3,soft)
  def invite(x):trace.append(('invite',x.g(3)&0xffffffff));x.setg(3,0)
  def mute(x):
   data=x.mem.rd(x.g(4)&0xffffffff,1);trace.append(('add',(x.g(3)+12)&0xffffffff,40,16,b'\0\0\0\1\0\0\0X\0\0\0\x0c'+data))
  for a,f in [(0x82360fd8,first),(0x8235f470,clear),(0x824ee528,watchclear),(0x824f87c0,watchappend),(0x82435188,add),(0x82436f10,apt),(0x82689a50,h),(0x82476c00,music),(0x823c0278,soften),(0x823c0288,invite),(0x82476bb0,mute)]:c.hooks[a]=f
  for n,v,sec in o.symbols.values():
   if (0,sec) not in b.bases:continue
   a=b.bases[(0,sec)]+v
   if '?GetFirstEvent@?$VariableEventQueue@$0EIAA@' in n:c.hooks[a]=first
   elif '?Clear@?$VariableEventQueue@$0EIAA@' in n:c.hooks[a]=clear
   elif '?AddEvent@?$VariableEventQueue@$0BAAAA@' in n:c.hooks[a]=add
  for a,n in b.unresolved.items():
   if '?gpBurnoutGame@@' in n:F.put(c,a,game)
   elif '?GetOutputEventQueue@StateInterface@' in n:c.hooks[a]=out
   elif '?ClearExpectedAptComponentList@GuiCache@' in n:c.hooks[a]=watchclear
   elif '?AppendExpectedAptComponent@GuiCache@' in n:c.hooks[a]=watchappend
   elif '?PlayAptMovie@StateInterface@' in n:c.hooks[a]=apt
   elif '?MakeHash@Name@Playback@CgsSound@@' in n:c.hooks[a]=h
   elif 'OutputGuiEvent@VGuiEventPlayMusicOnMenuStream' in n:c.hooks[a]=music
   elif '?HasGameBeenSoftRebooted@' in n:c.hooks[a]=soften
   elif '?HasGameBeenRebootedDueToInvite@' in n:c.hooks[a]=invite
  c.setg(3,base)
  try:c.run(b.entry if native else 0x82477270)
  except Exception:print('fail',i,native,hex(c.last_pc));raise
  outs.append((trace,c.mem.rd(base+0x48,4)))
 assert outs[0]==outs[1],(i,outs)
out={'passed':True,'cases':32,'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'Actual BootLegal::Update stage1 vs ARTIST 82477270 with empty input, cold and soft-reboot non-invite, compares real cache/component name arguments, queue/event trace, apt movie, music name/hash payload, game receiver and resulting stage. Queue, music, game and cache services controlled; initialized event bytes only.'};(D/'boot-stage1-comparison.json').write_text(json.dumps(out,indent=2)+'\n');print(out)
