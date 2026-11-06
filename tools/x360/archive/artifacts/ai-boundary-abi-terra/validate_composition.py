from pathlib import Path
import json,random,struct,hashlib
P=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');s=(P/'scratch/parallel-native/native_render_states/validate_hierarchy.py').read_text(encoding='utf8');s=s[:s.index('\nrows=')]+s[s.index('\ndef data_symbol'):s.index('\n# Decode')];__file__=str(P/'scratch/parallel-native/native_render_states/validate_hierarchy.py');exec(compile(s,__file__,'exec'))
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/ai-boundary-abi-terra');prov=out/'BrnAIBoundaryLine.obj';caller=out/'BrnResetOnTrackManager_Strategies.obj';cp=Coff(prov,'?GetInterp@BoundaryLine@BrnAI@@');cc=Coff(caller,'?ConvertNodesToPositionAndDirection@ResetOnTrackManager@BrnAI@@');rng=random.Random(0x82790420)
for case in range(64):
 lines=[struct.pack('>4f',*[rng.uniform(-1000,1000) for _ in range(4)]) for _ in range(2)];t=rng.uniform(-2,2);allres=[]
 for compiled in (False,True):
  c=cc.cpu();calls=[];addr1=M.S.SELF+0x400;addr2=M.S.SELF+0x420;c.mem.wr(addr1,lines[0]);c.mem.wr(addr2,lines[1]);c.setf(31,t);stack=c.g(1)&0xffffffff
  def boundary(cpu):cpu.setg(3,addr2)
  def interp(cpu):
   dest=cpu.g(3)&0xffffffff;line=cpu.g(4)&0xffffffff;calls.append((line,cpu.f(1)));pcpu=cp.cpu();pcpu.mem.wr(M.S.SELF,cpu.mem.rd(line,16));pcpu.mem.wr(M.S.SELF+0x100,bytes(16));pcpu.setg(3,M.S.SELF+0x100);pcpu.setg(4,M.S.SELF);pcpu.setf(1,cpu.f(1));pcpu.run(cp.entry);cpu.mem.wr(dest,pcpu.mem.rd(M.S.SELF+0x100,16));cpu.setg(3,dest)
  if compiled:
   c.setg(3,addr1);c.setg(31,0x12345670)
   for a,n in cc.unresolved.items():
    if n.startswith('?GetBoundaryLine@Portal@'):c.hooks[a]=boundary
    elif n.startswith('?GetInterp@BoundaryLine@'):c.hooks[a]=interp
   pc,end=cc.entry+0x128,cc.entry+0x154;dest=[stack+0x60,stack+0x70]
  else:
   c.setg(29,addr1);c.setg(28,addr2);pc,end=0x82790420,0x82790440;dest=[stack+0xa0,stack+0xb0]
   def originterp(cpu):
    line=cpu.g(4)&0xffffffff;calls.append((line,cpu.f(1)));cpu.call(0x82676c78,0)
   c.hooks[0x82676c78]=lambda cpu:None
   def original(cpu):
    line=cpu.g(4)&0xffffffff;calls.append((line,cpu.f(1)));saved=cpu.hooks.pop(0x82676c78);cpu.call(0x82676c78,0);cpu.hooks[0x82676c78]=saved
   c.hooks[0x82676c78]=original
  while pc!=end:
   word=c.fetch(pc);nxt=(pc+4)&0xffffffff
   if c.exec_vector(pc,word,''):pc=nxt
   else:pc=c.step(pc,word,nxt,0)
  allres.append((calls,[c.mem.rd(x,16) for x in dest]))
 assert allres[0]==allres[1],(case,allres)
report={'state':'pass','cases':64,'caller':str(caller),'caller_sha256':hashlib.sha256(caller.read_bytes()).hexdigest(),'provider':str(prov),'provider_sha256':hashlib.sha256(prov.read_bytes()).hexdigest(),'scope':'Real original and compiled caller instruction slices at the two GetInterp sites, calling actual original/compiled provider instructions with matching hidden-result ABI; GetBoundaryLine for second compiled call controlled, provider run through emulator service hook. Compares both line receivers, f1 and all output bytes. Valid finite values only; no full ResetOnTrack or gameplay claim.'};(out/'composition.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
