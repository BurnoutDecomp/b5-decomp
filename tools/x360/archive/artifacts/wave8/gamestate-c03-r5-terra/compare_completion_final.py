from pathlib import Path
import sys,json,random,struct,hashlib,math
P=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(P/'scratch/x360_sdk'));import native_atomic_fixture as F
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave8/gamestate-c03-r5-terra/final');row=next(x for x in json.loads((out/'compile-results.json').read_text())['results'] if x['source'].endswith('BrnProgressionManager_Completion.cpp'));obj=F.Coff(Path(row['object']),'?SendGameCompletionResults@ProgressionManager@BrnProgression@@');b=F.Bundle([obj]);b.select('?SendGameCompletionResults@ProgressionManager@BrnProgression@@');SELF=F.M.S.SELF;AUX=F.M.S.AUX;STACK=F.M.S.STACK;MODE=AUX+0x10000;QUEUE=AUX+0x1f000;MASK=0xffffffff

def branch(pc):
 w=F.M.V.x360rd.u32(pc);d=w&0x3fffffc;return (pc+(d-0x4000000 if d&0x2000000 else d))&MASK

def run(comp,pct,seen,dated,mode,seed):
 c=F.cpu(b);r=random.Random(seed);c.mem.wr(SELF,r.randbytes(0x20000));c.mem.wr(AUX,r.randbytes(0x10000));c.mem.wr(STACK,r.randbytes(0x20000));F.put(c,SELF+0x2093c,MODE);F.put(c,MODE+0xd94,mode);c.mem.wr(SELF+0x1ce80,bytes([seen]));c.mem.wr(SELF+0x1ce85,bytes([dated]));events=[];calls=[]
 def compute(x):x.setf(1,pct)
 def getter(x):x.setg(3,x.mem.rd(SELF+0x1ce80,1)[0])
 def update(x):calls.append(['date',x.g(3)&MASK])
 def add(x):events.append([x.g(3)&MASK,x.g(5)&MASK,x.g(6)&MASK,x.mem.rd(x.g(4)&MASK,x.g(6)&MASK).hex()]);x.setg(3,1)
 c.hooks[branch(0x82395c50)]=compute;c.hooks[F.address(b,'?ComputeCompletionPercentage@ProgressionManager')]=compute;c.hooks[branch(0x82395c9c)]=update;c.hooks[branch(0x82395ccc)]=add;c.hooks[F.address(b,'?AddEvent@?$VariableEventQueue')]=add
 for a,n in b.unresolved.items():
  if '?GetSeen100PercentCompletionSequence@' in n:c.hooks[a]=getter
  if '?Update@DateAndTime@' in n:c.hooks[a]=update
 c.setg(3,SELF);c.setg(4,QUEUE);c.run(b.entry if comp else 0x82395c28);return {'owner':(c.mem.rd(SELF,0x20000)+c.mem.rd(AUX,0xa00)).hex(),'events':events,'calls':calls}
errors=[];cases=0
for p in (0.0,99.0,100.0,101.0,float('nan')):
 for seen in (0,1):
  for dated in (0,1):
   for mode in (0,4,18,0xffffffff):
    a=run(False,p,seen,dated,mode,cases);c=run(True,p,seen,dated,mode,cases);cases+=1
    if a!=c:errors.append({'pct':str(p),'seen':seen,'dated':dated,'mode':mode,'fields':[k for k in a if a[k]!=c[k]],'events_original':a['events'],'events_native':c['events'],'calls_original':a['calls'],'calls_native':c['calls']})
result={'scope':'Original versus native actual SendGameCompletionResults; seeded owner and entire event bytes incl untouched padding; ComputeCompletionPercentage, Profile flag getter, DateAndTime Update and AddEvent controlled; compare owner, 8-byte payload, date/update order; real services excluded.','cases':cases,'mismatches':errors,'object':row['object'],'object_sha256':hashlib.sha256(Path(row['object']).read_bytes()).hexdigest()};(out/'completion-comparison.json').write_text(json.dumps(result,indent=2)+'\n');print('cases',cases,'mismatches',len(errors));print(json.dumps(errors[:3],indent=2));assert not errors

