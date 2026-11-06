from pathlib import Path
import json,random,sys,hashlib
P=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(P/'scratch/x360_sdk'));import native_atomic_fixture as F
O=Path('D:/Reverse/BP-X360-Experimental-Artifacts/director-526-terra');j=json.loads((O/'arbitrator-state-proposed/compile-results.json').read_text());objs={Path(x['source']).name:Path(x['object']) for x in j['results'] if x['passed']};sym='??0MainDirector@BrnDirector@@QAA@XZ';b=F.Bundle([F.Coff(objs['BrnMainDirector.cpp'],sym),F.Coff(objs['BrnDirectorArbitratorState.cpp'],'??0ArbitratorState@BrnDirector@@QAA@XZ')]);b.select(sym)
def target(pc):
 w=F.M.V.x360rd.u32(pc);d=w&0x03fffffc;d=d-0x04000000 if d&0x02000000 else d;return (pc+d)&0xffffffff
s=json.load(open('D:/Reverse/BP-Decomp_Workflow/.ida-exports/BURNOUT_X360_ARTIST.XEX/0x827E4AB8.json'))['assembly'].splitlines();SELF=F.M.S.SELF;rng=random.Random(0x1234a557); offsets=[0x12df0+0x170,0x166d0+0x170,0x16f60+0x170,0x17100+0x170]
for i in range(64):
 raw=[rng.randbytes(2) for _ in offsets];outs=[]
 for compiled in [False,True]:
  c=F.cpu(b);c.mem.add(SELF+0x20000,0x20000); c.mem.wr(SELF,b'\0'*0x20000);c.mem.wr(SELF+0x20000,b'\0'*0x20000); loc=[x-0x60 if compiled else x for x in offsets]
  for x,v in zip(loc,raw):c.mem.wr(SELF+x,v)
  c.setg(3,SELF)
  if compiled:
   for a in b.unresolved:c.hooks[a]=lambda cc:None
   for n,(oi,v,sec) in b.definitions.items():
    if (oi,sec) in b.bases and any(q in n for q in ['??0BehaviourManager@','??0ICEMoviePlaylist@']):c.hooks[b.bases[(oi,sec)]+v]=lambda cc:None
   c.run(b.entry)
  else:
   for l in s:
    if '  bl ' not in l:continue
    p=l.split();
    if p[2].startswith('__save') or p[2].startswith('__rest'):continue
    c.hooks[target(int(p[0],16))]=lambda cc:None
   c.run(0x827e4ab8)
  outs.append([c.mem.rd(SELF+x,2) for x in loc])
 assert outs==[raw,raw],(i,[x.hex() for x in raw],[[x.hex() for x in z] for z in outs])
r={'cases':64,'states':['testbed','crashnav','attract','render metrics'],'objects':{n:{'path':str(p),'sha256':hashlib.sha256(p.read_bytes()).hexdigest()} for n,p in objs.items() if n in ['BrnMainDirector.cpp','BrnDirectorArbitratorState.cpp']},'scope':'Original and actual compiled MainDirector constructors; compare only two flag bytes in four inline arbitrator states. External and unrelated constructors mocked; BehaviourManager and movie playlist mocked on candidate. Candidate addresses adjusted by measured -0x60 ICE drift. Other state fields and full Main ctor not compared.'};(O/'arbitrator-flags-comparison.json').write_text(json.dumps(r,indent=2)+'\n');print(json.dumps(r,indent=2))
