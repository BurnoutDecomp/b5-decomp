from pathlib import Path
import sys,json,random,hashlib
P=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(P/'scratch/x360_sdk'));import native_atomic_fixture as F
O=Path('D:/Reverse/BP-X360-Experimental-Artifacts/director-526-terra');j=json.loads((O/'ice-geometry-overlay/compile-results.json').read_text());obj=next(Path(x['object']) for x in j['results'] if x['source'].endswith('DirectorModule/BrnDirectorModule.cpp'));sym='?PreSceneQueryUpdate@DirectorModule@BrnDirector@@';b=F.Bundle([F.Coff(obj,sym)]);b.select(sym)
SELF=F.M.S.SELF;AUX=F.M.S.AUX;QUERY=AUX+0x4000
orig=json.load(open('D:/Reverse/BP-Decomp_Workflow/.ida-exports/BURNOUT_X360_ARTIST.XEX/0x8225C768.json'))['assembly'].splitlines()
def target(pc):
 w=F.M.V.x360rd.u32(pc);d=w&0x03fffffc;d=d-0x04000000 if d&0x02000000 else d;return (pc+d)&0xffffffff
def call(c,name,trace):
 if 'SceneQueryOutputB' in name or 'GetSceneQueryInterface@SceneQueryOutputBuffer' in name:c.setg(3,QUERY)
 elif 'MainDirector__PreSceneQueryUpdate' in name or '?PreSceneQueryUpdate@MainDirector' in name:trace.append('main')
 elif 'ReplayDirector__PreSceneQueryUpdate' in name:trace.append('replay')
 elif 'DirectorSerialiser__Read' in name:trace.append('read')
 elif 'GetStaticLayout' in name:c.setg(3,AUX+0x3000);trace.append('layout')
 elif 'StopMonitor' in name:c.setg(3,0)
 elif any(x in name for x in ['BeginAssert','FireAssert','EndAssert']):raise AssertionError(('assert',name,hex(c.last_pc)))
rng=random.Random(0x8225c768);results=[]; count=0
for replay in [0,1]:
 for mode in range(8):
  if replay and mode in (4,5,6):continue
  for i in range(16):
   initial=[rng.randrange(256) for _ in range(5)];initial[0] &=1;both=[]
   for compiled in [False,True]:
    c=F.cpu(b); c.mem.add(SELF+0x20000,0x20000);c.mem.wr(SELF,b'\0'*0x20000);c.mem.wr(SELF+0x20000,b'\0'*0x20000);trace=[]
    serial=SELF+(0x388e0 if compiled else 0x38930);flags=SELF+(0x38ab0 if compiled else 0x38b00);reset=SELF+(0x388d4 if compiled else 0x38924)
    c.mem.wr(serial,mode.to_bytes(4,'big'));c.mem.wr(flags,bytes(initial[:2]));c.mem.wr(reset,bytes([initial[2]]));c.mem.wr(serial+0x58,bytes(initial[3:5]));
    for k,v in [(3,SELF),(4,0),(5,0),(6,AUX+0x6000),(7,AUX+0x7000),(8,AUX+0x8000),(9,replay*0x100)]:c.setg(k,v)
    if compiled:
     for at,n in b.unresolved.items():c.hooks[at]=lambda cc,name=n:call(cc,name,trace)
     c.run(b.entry)
    else:
     for line in orig:
      if '  bl ' not in line:continue
      parts=line.split();pc=int(parts[0],16);name=parts[2]
      if name.startswith('__save') or name.startswith('__rest'):continue
      c.hooks[target(pc)]=lambda cc,n=name:call(cc,n,trace)
     c.run(0x8225C768)
    both.append((c.mem.rd(flags,2),c.mem.rd(reset,1),c.mem.rd(serial+0x58,2),trace))
   if both[0]!=both[1]:raise AssertionError((replay,mode,i,both))
   count+=1
  results.append({'replay':replay,'mode':mode,'cases':16,'trace':both[0][3]})
r={'cases':count,'results':results,'object':str(obj),'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'Full original vs compiled PreSceneQueryUpdate under mocked IO/monitor/main services; compare replay flags, reset latch, serialiser data-ready/restored and main handoff across 13 mode/replay combinations. Replay-active playback modes4/5/6 excluded because current implementation is gated; candidate source offset adjusted for measured -0x50 ICE drift. No full gameplay claim.'};(O/'pre-scene-comparison-ice.json').write_text(json.dumps(r,indent=2)+'\n');print('passed',count)
