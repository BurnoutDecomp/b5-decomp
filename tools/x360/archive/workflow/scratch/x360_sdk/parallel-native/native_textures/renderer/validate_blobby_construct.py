from pathlib import Path
import hashlib,json,random,sys
parent=Path(r'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(parent/'scratch/x360_sdk'));import native_atomic_fixture as F
out=parent/'b5-decomp/build/x360/parallel/native_textures/renderer-blobby-construction3';r=json.loads((out/'compile-results.json').read_text())['results'][0];p=Path(r['object']);o=F.Coff(p,'?Construct@BrnBlobbyShadowManager@@');SELF=F.M.S.SELF;rng=random.Random(0x8240ACD0);errors=[]
class Stop(Exception):pass
for i in range(128):
 image=rng.randbytes(0xca80);both=[]
 for compiled in [False,True]:
  c=F.cpu(o);c.mem.wr(SELF,image)
  if compiled:c.setg(3,SELF+0x17c0);c.run(o.entry)
  else:
   c.setg(31,SELF);c.setg(30,0);c.setg(28,1)
   def observe(pc,word):
    if pc==0x8240ACE0:raise Stop()
   c.instruction_observer=observe
   try:c.run(0x8240ACD0)
   except Stop:pass
  both.append(c.mem.rd(SELF+0x17c0,0x2030))
 if both[0]!=both[1]:errors.append(dict(case=i,different_bytes=[hex(n) for n,(a,b) in enumerate(zip(both[0],both[1])) if a!=b]))
result=dict(cases=128,errors=errors,object=str(p),sha256=hashlib.sha256(p.read_bytes()).hexdigest(),scope='actual whole-CPP native BrnBlobbyShadowManager::Construct against ARTIST8240ACD0..ACDC,all2030receiverbytes:bothcounts0,internal0,external1,allotherbytespreserved',limits='onlyConstructrecovered;othermethodsreceivedincludeandC++03colorassignmentspellingfixesonlyandarenotbehaviorverified')
(out/'blobby-constructor-comparison.json').write_text(json.dumps(result,indent=2)+'\n');print('128 native blobby Construct cases; errors',len(errors));raise SystemExit(bool(errors))
