from pathlib import Path
import sys,json,random,struct,hashlib
P=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(P/'scratch/x360_sdk'));import native_atomic_fixture as F
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave8/gamestate-c03-r5-terra/probe');row=next(x for x in json.loads((out/'compile-results.json').read_text())['results'] if x['source'].endswith('BrnGameStateStreetManager_wC_00.cpp'));obj=F.Coff(Path(row['object']),'?UpdateBufferedHighScores@StreetManager@BrnGameState@@');b=F.Bundle([obj]);b.select('?UpdateBufferedHighScores@StreetManager@BrnGameState@@');SELF=F.M.S.SELF;AUX=F.M.S.AUX;STACK=F.M.S.STACK;OUTPUT=AUX+0x1000;QUEUE=AUX+0x2000;MASK=0xffffffff

def branch(pc):
 w=F.M.V.x360rd.u32(pc);d=w&0x3fffffc;return (pc+(d-0x4000000 if d&0x2000000 else d))&MASK

def run(comp,summary,lost,score,seed):
 c=F.cpu(b);r=random.Random(seed);c.mem.wr(SELF,r.randbytes(0x2000));c.mem.wr(STACK,r.randbytes(0x20000));F.put(c,SELF+0x1ca0,1);c.mem.wr(SELF+0x1ca8,b'\0'*8);F.put(c,SELF+0x1cc0,0);F.put(c,SELF+0x1cbc,lost);F.put(c,SELF+0x1cb8,0x42c80000);c.mem.wr(SELF+0x1cc4,bytes([summary]));events=[];trace=[]
 def gui(x):x.setg(3,QUEUE)
 def action(x):x.setg(3,QUEUE)
 def roads(x):trace.append(['roads',x.g(3)&MASK]);x.setg(3,score)
 def get(x):x.setg(3,SELF+0x1c00)
 def erase(x):F.put(x,SELF+0x1ca0,0)
 def add(x):events.append([x.g(3)&MASK,x.g(5)&MASK,x.g(6)&MASK,x.mem.rd(x.g(4)&MASK,x.g(6)&MASK).hex()]);x.setg(3,1)
 def copy(x):d=x.g(3)&MASK;s=x.g(4)&MASK;n=x.g(5)&MASK;x.mem.wr(d,x.mem.rd(s,n));x.setg(3,d)
 for pc,fn in ((0x823494cc,roads),(0x823495b0,roads),(0x823494e0,gui),(0x823495cc,action),(0x823495f4,gui),(0x823494f0,add),(0x82349538,copy),(0x82349528,get),(0x823495c4,erase)):c.hooks[branch(pc)]=fn
 c.hooks[F.address(b,'?AddEvent@?$VariableEventQueue')]=add;c.hooks[F.address(b,'??A?$Array@UBufferedNewHighScore')]=get;c.hooks[F.address(b,'?Erase@?$Array@UBufferedNewHighScore')]=erase
 for a,n in b.unresolved.items():
  for fragment,fn in (('GetNumberOfRoadsRulesByLocalPlayer',roads),('GetGuiOutputQueue',gui),('GetGameActionQueue',action),('memcpy',copy)):
   if fragment in n:c.hooks[a]=fn
 c.setg(3,SELF);c.setf(1,0.25);c.setg(5,0);c.setg(6,OUTPUT);c.run(b.entry if comp else 0x823493f0);return {'owner':c.mem.rd(SELF,0x2000).hex(),'events':events,'calls':trace}
errors=[];case=0
for summary in (0,1):
 for lost in (0,3,0xffffffff):
  for score in (0,5,0x7fffffff):
   for z in range(8):
    a=run(False,summary,lost,score,case);c=run(True,summary,lost,score,case);case+=1
    if a!=c:errors.append({'summary':summary,'lost':lost,'score':score,'fields':[k for k in a if a[k]!=c[k]],'original_events':a['events'],'native_events':c['events'],'orig_calls':a['calls'],'native_calls':c['calls']})
result={'scope':'Original vs native actual UpdateBufferedHighScores, emitted summary and single record with seeded stack bytes; output queue, array get/erase, memcpy, and road count controlled; owner bytes, 48-byte payload incl untouched bytes, calls/order compared. No real queue/services or complete StreetManager parity.','cases':case,'mismatches':errors,'object':row['object'],'object_sha256':hashlib.sha256(Path(row['object']).read_bytes()).hexdigest()};(out/'highscore-comparison.json').write_text(json.dumps(result,indent=2)+'\n');print('cases',case,'mismatches',len(errors));print(json.dumps(errors[:1],indent=2));assert not errors
