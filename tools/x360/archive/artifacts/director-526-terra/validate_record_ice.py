from pathlib import Path
import sys,json,random,struct,hashlib
P=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(P/'scratch/x360_sdk'));import native_atomic_fixture as F
O=Path('D:/Reverse/BP-X360-Experimental-Artifacts/director-526-terra');j=json.loads((O/'ice-geometry-overlay/compile-results.json').read_text());obj=next(Path(x['object']) for x in j['results'] if x['source'].endswith('DirectorModule/BrnDirectorModule.cpp'));sym='?Update@DirectorModule@BrnDirector@@';b=F.Bundle([F.Coff(obj,sym)]);b.select(sym)
SELF=F.M.S.SELF; AUX=F.M.S.AUX; FRAME=AUX+0x3000; QUERY=AUX+0x4000; REQUEST=AUX+0x5000
orig= json.load(open('D:/Reverse/BP-Decomp_Workflow/.ida-exports/BURNOUT_X360_ARTIST.XEX/0x82275300.json'))['assembly'].splitlines()
def target(pc):
 w=F.M.V.x360rd.u32(pc);d=w&0x03fffffc;d=d-0x04000000 if d&0x02000000 else d;return (pc+d)&0xffffffff
def effects(c,name,trace):
 if 'GetStaticLayout' in name:c.setg(3,FRAME);trace.append('layout')
 elif 'SceneQueryOutputB' in name or 'GetSceneQueryInterface@SceneQueryOutputBuffer' in name:c.setg(3,QUERY)
 elif 'GetReplayRequestI' in name:c.setg(3,REQUEST)
 elif 'HasData' in name:c.setg(3,0)
 elif 'BrnDirector__Camera__Camera__operator_' in name or ('??4Camera@' in name and 'BrnDirector' in name):c.mem.wr(c.g(3),c.mem.rd(c.g(4),0x160));trace.append('camera_copy')
 elif 'Camera__Construct' in name or ('?Construct@Camera@1BrnDirector' in name):trace.append('camera_construct')
 elif 'DirectorSerialiser__Write' in name or '?Write@DirectorSerialiser' in name:trace.append('write')
 elif 'RegisterSerialiser' in name:trace.append('register')
 elif any(x in name for x in ['BeginAssert','FireAssert','EndAssert']):raise AssertionError(('unexpected assert',name,hex(c.last_pc)))
 elif 'StopMonitor' in name:c.setg(3,0)
# The real constructor headers differ by the known -0x60 ICE displacement; fill corresponding sources.
rng=random.Random(0x82275300); records=[]
for mode in [0,1,2,3,4,5,6,7]:
 for i in range(16):
  vals={'cam':rng.randbytes(0x160),'key':rng.randbytes(8),'boost':rng.randbytes(8),'speed':rng.randbytes(4),'hand':struct.pack('>f',rng.uniform(-10,10))}; frame0=rng.randbytes(0x180); both=[]
  for compiled in [False,True]:
   c=F.cpu(b); c.mem.add(SELF+0x20000,0x20000); c.mem.wr(SELF,b'\0'*0x20000); c.mem.wr(SELF+0x20000,b'\0'*0x20000); c.mem.wr(FRAME,frame0); trace=[]
   serial=SELF+(0x388e0 if compiled else 0x38930); c.mem.wr(serial,struct.pack('>I',mode));
   bm=SELF+0xb00+(0x1cab0 if compiled else 0x1cb10); last=SELF+0xb00+(0x32ec0 if compiled else 0x32f10)
   for ptr,data in [(last,vals['cam']),(bm+0x149b0,vals['key']),(bm+0x1586c,vals['boost']),(bm+0x15874,vals['speed']),(bm+0x163c0,vals['hand'])]:c.mem.wr(ptr,data)
   for k,v in [(3,SELF),(4,0),(5,0),(6,AUX+0x6000),(7,AUX+0x7000),(8,AUX+0x8000),(9,AUX+0x9000)]:c.setg(k,v)
   if compiled:
    for at,name in b.unresolved.items():c.hooks[at]=lambda cc,n=name:effects(cc,n,trace)
    for name,(oi,v,sec) in b.definitions.items():
     if (oi,sec) in b.bases and ('?GetStaticLayout@DirectorSerialiser' in name or '?ProcessSceneQueryResults@DirectorModule' in name or ('??4Camera@' in name and 'BrnDirector' in name)):
      c.hooks[b.bases[(oi,sec)]+v]=lambda cc,n=name:effects(cc,n,trace)
    c.run(b.entry)
   else:
    for line in orig:
     if '  bl ' not in line:continue
     parts=line.split(); pc=int(parts[0],16);name=parts[2]
     if name.startswith('__save') or name.startswith('__rest'):continue
     c.hooks[target(pc)]=lambda cc,n=name:effects(cc,n,trace)
    c.run(0x82275300)
   both.append((c.mem.rd(FRAME,0x180),trace))
  if both[0]!=both[1]:raise AssertionError((mode,i,both[0][1],both[1][1],[hex(k) for k,(x,y) in enumerate(zip(both[0][0],both[1][0])) if x!=y][:15]))
 records.append({'mode':mode,'cases':16,'trace':both[0][1]})
r={'cases':128,'modes':records,'object':str(obj),'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'Full original and compiled DirectorModule::Update under controlled service hooks, replay=false; compare entire 0x180 frame and camera-construct/copy, six layout, write and registration calls. Source positions adjusted for known -0x50 ICE layout drift. External services mocked; no full game or playback proof.'};(O/'record-comparison-ice.json').write_text(json.dumps(r,indent=2)+'\n');print(json.dumps(r,indent=2))




