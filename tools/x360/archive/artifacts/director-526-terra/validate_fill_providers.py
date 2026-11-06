from pathlib import Path
import sys,json,random,struct,hashlib
P=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(P/'scratch/x360_sdk'));import native_atomic_fixture as F
O=Path('D:/Reverse/BP-X360-Experimental-Artifacts/director-526-terra');j=json.loads((O/'fill-providers/compile-results.json').read_text()); paths={Path(x['source']).name:Path(x['object']) for x in j['results'] if x['passed']};obj=paths['BrnReplayDirector.cpp'];sym='?FillInSharedInfo@ReplayDirector@BrnDirector@@';b=F.Bundle([F.Coff(obj,sym),F.Coff(paths['BrnCamera2DRotationController.cpp'],'?Construct@Camera2DRotationController'),F.Coff(paths['BrnCameraSphericalRotationController.cpp'],'?Construct@CameraSphericalRotationController'),F.Coff(paths['BrnDirectorModuleDebugPrinter.cpp'],'?Construct@DebugLog@BrnDirector@@')]);b.select(sym)
S=F.M.S.SELF;A=F.M.S.AUX; IN=A;BEH=A+0x10000;COL=A+0x11000;FRAME=A+0x12000;IO=A+0x13000;TIMER=IN+0x6750;CTRL=IN+0x3260;RACE=IN+0x990;USED=IN+0x980;TEAM=IN+0x3238;TRAFFIC=IN+0x6ad0
orig=json.load(open('D:/Reverse/BP-Decomp_Workflow/.ida-exports/BURNOUT_X360_ARTIST.XEX/0x82250558.json'))['assembly'].splitlines()
def target(pc):
 w=F.M.V.x360rd.u32(pc);d=w&0x03fffffc;d=d-0x04000000 if d&0x02000000 else d;return (pc+d)&0xffffffff
def u32(c,a,v):c.mem.wr(a,struct.pack('>I',v&0xffffffff))
def effects(c,n,trace,player,nearest):
 if 'GetPlayerCarIndex' in n:c.setg(3,player);trace.append('player')
 elif 'GetUsedRaceCars' in n:c.setg(3,USED);trace.append('used')
 elif 'GetVehicleInfoArray' in n or 'GetVehi' in n:c.setg(3,TEAM);trace.append('teams')
 elif 'GetRaceCarInfo' in n or 'sub_82207040' in n:c.setg(3,RACE);trace.append('races')
 elif 'GetTrafficDirectorEntityArray' in n:c.setg(3,TRAFFIC)
 elif 'Update@AllVehicleData' in n or 'AllVehicleData__Update' in n:trace.append(('all_update',c.g(4),c.g(5),c.g(6),c.g(7),c.g(8)))
 elif 'Clear@GameState' in n or 'GameState__Clear' in n:trace.append('game_clear')
 elif 'Update@VehicleTracker' in n or 'VehicleTracker__Update' in n:trace.append('tracker_update')
 elif 'Construct@Camera2DRotationController' in n:
  p=c.g(3);c.mem.wr(p,b'\0'*16);c.mem.wr(p+0x10,struct.pack('>5f',0,.4,0,.1,.125));c.mem.wr(p+0x24,b'\0'*3)
 elif 'Construct@CameraSphericalRotationController' in n:
  p=c.g(3);c.mem.wr(p,b'\0'*0x20);c.mem.wr(p+0x20,b'\0'*3);c.mem.wr(p+0x28,b'\0'*8)
 elif 'Construct@DebugLog' in n:
  p=c.g(3);c.mem.wr(p,struct.pack('>f',10));c.mem.wr(p+0x600,b'\0'*8);c.mem.wr(p+0x658,b'\0'*4)
  for i in range(20):u32(c,p+0x5a8+i*4,19-i)
  u32(c,p+0x5f8,20)
 elif 'Construct@DebugPrinter' in n or 'DebugPrinter__Construct' in n:trace.append('printer')
 elif 'GetTimerStatusInter' in n:c.setg(3,TIMER);trace.append('timer')
 elif 'GetNearestRaceCarIndexToPlayer' in n:c.setg(3,nearest);trace.append('nearest')
 elif 'GetRaceCar@AllVehicleData' in n or 'AllVehicleData__GetRaceCar' in n:c.setg(3,RACE+c.g(4)*0x4f0);trace.append('racecar')
 elif 'GetPlayer@AllVehicleData' in n or 'AllVehicleData__GetPlayer' in n:c.setg(3,RACE+player*0x4f0);trace.append('playercar')
 elif 'CameraSpaceHandler' in n and 'Construct' in n:
  args=[c.g(k) for k in range(4,11)]+[c.rd32(c.g(1)+0x54)];trace.append(('spaces',[c.mem.rd(x,64).hex() for x in args],c.rd32(c.g(1)+0x5c)))
 elif 'GetControll' in n:c.setg(3,CTRL);trace.append('controller')
 elif 'VehicleInfo__operator_' in n or '??4VehicleInfo@' in n:
  dst,src=c.g(3),c.g(4);c.mem.wr(dst,c.mem.rd(src,0x4f0));c.setg(3,dst);trace.append('vehicle_copy')
 elif 'SineLerp' in n:trace.append(('sine',c.f(1),c.f(2),c.f(3)));c.setf(1,0.625)
 elif any(x in n for x in ['BeginAssert','FireAssert','EndAssert']):raise AssertionError(('assert',n,hex(c.last_pc)))
 else:raise AssertionError(('unexpected',n))
def run(compiled,seed,pause,boost,vel,game,player,nearest,mask,streamed,lookback,carvec,camvec):
 c=F.cpu(b);c.mem.wr(S,seed[0]);c.mem.wr(IN,seed[1]);c.mem.wr(BEH,seed[2]);c.mem.wr(COL,seed[3]);c.mem.wr(FRAME,seed[4]);
 c.mem.wr(IO,struct.pack('>5I',IN,A+0x9000,A+0xa000,A+0xb000,A+0xc000));c.mem.wr(USED,mask.to_bytes(8,'big'));c.mem.wr(IN+0x7ac8,bytes([pause]));c.mem.wr(IN+0x6ab4,bytes([streamed]));c.mem.wr(CTRL+4,bytes([lookback]));c.mem.wr(CTRL+0x10,carvec);c.mem.wr(CTRL+0x20,camvec);
 c.mem.wr(TIMER+4,struct.pack('>f',game));c.mem.wr(TIMER+8,struct.pack('>f',.5));c.mem.wr(TIMER+12,b'\1');c.mem.wr(TIMER+28,struct.pack('>f',vel));c.mem.wr(TIMER+32,struct.pack('>f',.75));c.mem.wr(FRAME+0x16c,struct.pack('>f',boost));trace=[]
 for k,v in [(3,S),(4,BEH),(5,COL),(6,FRAME),(7,IO)]:c.setg(k,v)
 if compiled:
  for a,n in b.unresolved.items():c.hooks[a]=lambda cc,name=n:effects(cc,name,trace,player,nearest)
  for n,(oi,v,sec) in b.definitions.items():
   if (oi,sec) in b.bases and any(x in n for x in ['?Update@AllVehicleData','?GetNearestRaceCarIndexToPlayer@AllVehicleData','?GetRaceCar@AllVehicleData','?GetPlayer@AllVehicleData','?Construct@DebugPrinter@']):c.hooks[b.bases[(oi,sec)]+v]=lambda cc,name=n:effects(cc,name,trace,player,nearest)
  c.run(b.entry)
 else:
  for l in orig:
   if '  bl ' not in l:continue
   p=l.split();pc=int(p[0],16);n=p[2]
   if n.startswith('__save') or n.startswith('__rest') or 'AllVehicleData__Construct' in n:continue
   c.hooks[target(pc)]=lambda cc,name=n:effects(cc,name,trace,player,nearest)
  c.run(0x82250558)
 return c.mem.rd(S,0x29e0),c.mem.rd(BEH,0x610),c.mem.rd(COL,0x70),trace,c.g(3)
rng=random.Random(0x82250558);count=0
for pause in (0,1):
 for i in range(64):
  seed=[rng.randbytes(n) for n in (0x29e0,0x8000,0x610,0x70,0x180)]
  player=i%8;nearest=(i*5+3)%8;mask=(1<<player)|rng.randrange(256);streamed=i&1;lookback=(i>>1)&1
  boost=[0.0,.25,1.0,-1.0,1e-30][i%5];vel=[0.0,.125,.5,1.0][i%4];game=[.25,.5,1.0][i%3]
  carvec=rng.randbytes(16);camvec=rng.randbytes(16)
  args=(seed,pause,boost,vel,game,player,nearest,mask,streamed,lookback,carvec,camvec)
  a=run(False,*args);z=run(True,*args)
  if a!=z:
   print('trace original',a[3]);print('trace candidate',z[3])
   for name,x,y in zip(['self','behaviour','collision'],a[:3],z[:3]): print(name,[hex(k) for k,(v,w) in enumerate(zip(x,y)) if v!=w][:80])
   print('return',hex(a[4]),hex(z[4]));raise AssertionError(('mismatch',pause,i))
  count+=1
r={'cases':count,'object':str(obj),'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'Actual ARTIST 82250558 vs compiled FillInSharedInfo on randomized full replay/input/frame/outputs for player and nearest indices, used masks, controller vector bits, timer values, pause, streaming and lookback. Compare full ReplayDirector0x29E0, BehaviourSharedInfo0x610, CollisionPolicySharedInfo0x70, return pointer and ordered relevant service calls/arguments including camera-space matrices. AllVehicleData::Construct and Random::Construct execute real code; rotation and debug-log constructors execute their actual compiled providers and world/vehicle/timer getter, AllVehicleData::Update/Get*, GameState::Clear, VehicleTracker::Update, DebugPrinter::Construct, CameraSpaceHandler::Construct, VehicleInfo assignment and SineLerp are mocked. No full gameplay or provider behavior claim.'};(O/'fill-comparison-providers.json').write_text(json.dumps(r,indent=2)+'\n');print('passed',count)
