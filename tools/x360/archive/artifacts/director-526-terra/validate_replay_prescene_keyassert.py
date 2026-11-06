from pathlib import Path
import sys,json,random,struct,hashlib
P=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(P/'scratch/x360_sdk'));import native_atomic_fixture as F
O=Path('D:/Reverse/BP-X360-Experimental-Artifacts/director-526-terra');j=json.loads((O/'replay-prescene-wip3/compile-results.json').read_text());obj=next(Path(x['object']) for x in j['results'] if x['source'].endswith('BrnReplayDirector.cpp'));sym='?PreSceneQueryUpdate@ReplayDirector@BrnDirector@@';pj=json.loads((O/'parameters-typed-wip4/compile-results.json').read_text());pp={Path(x['source']).name:Path(x['object']) for x in pj['results'] if x['passed']};b=F.Bundle([F.Coff(obj,sym),F.Coff(pp['BrnBehaviourGameplayExternal.cpp'],'?Set@Parameters@BehaviourGameplayExternal@'),F.Coff(pp['BrnBehaviourGameplayBumper.cpp'],'?Set@Parameters@BehaviourGameplayBumper@')]);b.select(sym)
S=F.M.S.SELF;A=F.M.S.AUX;IO=A+0x1000;IN=A+0x2000;FRAME=A+0x4000;USED=A+0x5000;VT=A+0x7000;POL=A+0x8000;PVT=A+0x9000;CAR=A+0xa000;BUM=A+0xb000;EXT=A+0xc000;NAME=A+0xd000
VCONS=0x84f00000;VPREP=0x84f00010;VUP=0x84f00020;VGET=0x84f00030;VQUERY=0x84f00040
orig=json.load(open('D:/Reverse/BP-Decomp_Workflow/.ida-exports/BURNOUT_X360_ARTIST.XEX/0x8225BD28.json'))['assembly'].splitlines()
def target(pc):
 w=F.M.V.x360rd.u32(pc);d=w&0x03fffffc;d=d-0x04000000 if d&0x02000000 else d;return (pc+d)&0xffffffff
def put(c,a,x):c.mem.wr(a,struct.pack('>I',x&0xffffffff))
class CPU(F.AtomicCPU):
 def step(self,pc,w,nxt,depth):
  if w>>26==31 and ((w>>1)&1023)==27:
   rs=(w>>21)&31;ra=(w>>16)&31;rb=(w>>11)&31;shift=self.g(rb)&127;self.setg(ra,(self.g(rs)<<shift)&0xffffffffffffffff if shift<64 else 0);return nxt
  return super().step(pc,w,nxt,depth)
def run(compiled,initial,player,used,reset,mask,key):
 c=F.cpu(b);c.__class__=CPU;c.mem.wr(S,initial);c.mem.wr(IO,struct.pack('>5I',IN,A+0x3000,A+0xe000,A+0xf000,A+0x11000));c.mem.wr(USED,(1<<player if used and player>=0 else 0).to_bytes(8,'big'));c.mem.wr(FRAME+0x160,struct.pack('>Q',key));c.mem.wr(S+0x29d4,bytes([reset]));put(c,S+0x690,VT);put(c,S+0x1200,VT)
 for offset,ptr in [(0,VCONS),(4,VPREP),(8,VUP),(20,VGET)]:put(c,VT+offset,ptr)
 put(c,POL,PVT);put(c,PVT,VQUERY);put(c,CAR+0x1e8,NAME);c.mem.wr(BUM,struct.pack('>17f',*([1.0]*17)));c.mem.wr(EXT,struct.pack('>17f',*([1.0]*17)));c.mem.wr(BUM+24,struct.pack('>f',45.0));c.mem.wr(EXT+64,struct.pack('>f',55.0));t=[]
 def receiver(cc):return 0 if cc.g(3)==S+0x690 else 1
 def effects(cc,n):
  if 'GetPlayerCarIndex' in n:cc.setg(3,player);t.append('player')
  elif 'GetUsedRaceCars' in n:cc.setg(3,USED);t.append('used')
  elif 'burnoutcarasset@' in n or 'sub_82204998' in n:
   t.append(('car',cc.g(4),cc.g(5)));put(cc,cc.g(3),0x12340000);put(cc,cc.g(3)+4,CAR)
  elif 'GetCollection' in n:
   which='bumper' if cc.g(3)==CAR+0x1b8 else 'external' if cc.g(3)==CAR+0x1a0 else ('badref',hex(cc.g(3)));t.append(('collection',which));cc.setg(3,0x11110000 if which=='bumper' else 0x22220000)
  elif 'camerabumperbehaviour' in n and ('??0' in n or '__camerabumperbehaviour' in n):t.append(('asset','bumper',cc.g(4),cc.g(5)));put(cc,cc.g(3),0x11110000);put(cc,cc.g(3)+4,BUM)
  elif 'cameraexternalbehaviour' in n and ('??0' in n or '__cameraexternalbehaviour' in n):t.append(('asset','external',cc.g(4),cc.g(5)));put(cc,cc.g(3),0x22220000);put(cc,cc.g(3)+4,EXT)
  elif ('Parameters__Set' in n or '?Set@Parameters@BehaviourGameplay' in n):
   which='external' if 'External' in n else 'bumper';t.append(('params',which,cc.g(3)-S,cc.rd32(cc.g(4)+4)))
  elif 'SetParameters' in n:t.append(('bind',cc.g(3)-S,cc.g(4)-S))
  elif 'Camera__Construct' in n or '?Construct@Camera@' in n:t.append(('camera',cc.g(3)-S))
  elif 'GameState__Clear' in n or '?Clear@GameState@' in n:t.append('game_clear')
  elif 'VehicleTracker__Construct' in n or '?Construct@VehicleTracker@' in n:t.append('tracker_construct')
  elif 'VehicleTracker__Update' in n or '?Update@VehicleTracker@' in n:t.append(('tracker_update',cc.g(3)-S,cc.g(4)-S,cc.g(5),cc.g(6),cc.g(7)))
  elif 'Instance___Instance' in n or '??1Instance@Attrib' in n:t.append('destroy')
  elif 'FillInSharedInfo' in n:assert cc.g(6)==FRAME and cc.g(7)==IO;t.append('fill')
  elif 'BeginAssert' in n:t.append('assert_begin')
  elif 'FireAssert' in n:t.append(('assert_fire',cc.g(5)))
  elif 'EndAssert' in n:t.append('assert_end')
  else:raise AssertionError(('unhandled',n))
 def construct(cc):t.append(('construct',receiver(cc)))
 def prepare(cc):t.append(('prepare',receiver(cc),cc.rd32(cc.g(4)),cc.rd32(cc.g(4)+4)))
 def update(cc):t.append(('update',receiver(cc),cc.g(4)-S))
 def get(cc):
  index=receiver(cc);t.append(('getpolicy',index));cc.setg(3,POL if mask&(1<<index) else 0)
 def query(cc):t.append(('query',cc.g(5)-S))
 c.hooks.update({VCONS:construct,VPREP:prepare,VUP:update,VGET:get,VQUERY:query})
 for k,v in [(3,S),(4,FRAME),(5,IO)]:c.setg(k,v)
 if compiled:
  for at,n in b.unresolved.items():c.hooks[at]=lambda cc,name=n:effects(cc,name)
  for n,(oi,v,sec) in b.definitions.items():
   if (oi,sec) in b.bases and any(x in n for x in ['??0burnoutcarasset@','??0camerabumperbehaviour@','??0cameraexternalbehaviour@','?FillInSharedInfo@ReplayDirector','?SetParameters@BehaviourGameplayBumper','?SetParameters@BehaviourGameplayExternal']):c.hooks[b.bases[(oi,sec)]+v]=lambda cc,name=n:effects(cc,name)
  c.run(b.entry)
 else:
  for l in orig:
   if '  bl ' not in l:continue
   p=l.split();pc=int(p[0],16);n=p[2]
   if n.startswith('__save') or n.startswith('__rest'):continue
   if 'Parameters__Set' in n:continue
   c.hooks[target(pc)]=lambda cc,name=n:effects(cc,name)
  c.run(0x8225bd28)
 result=bytearray(c.mem.rd(S,0x29e0));result[0x594:0x598]=bytes(4);result[0x644:0x648]=bytes(4);return bytes(result),t
rng=random.Random(0x8225bd28);count=0
for reset in (0,1):
 for mask in range(4):
  for i in range(16):
   player=i%8;args=(rng.randbytes(0x29e0),player,True,reset,mask,(0 if i==0 else (0x100000000 if i==1 else rng.randrange(1,2**63))));a=run(False,*args);z=run(True,*args)
   if a!=z:
    print('original trace',a[1]);print('candidate trace',z[1]);print('differences',[hex(k) for k,(x,y) in enumerate(zip(a[0],z[0])) if x!=y][:100]);raise AssertionError((reset,mask,i))
   count+=1
for player,used in [(-1,False),(0,False),(7,False)]:
 args=(rng.randbytes(0x29e0),player,used,1,3,7);assert run(False,*args)==run(True,*args);count+=1
r={'cases':count,'object':str(obj),'sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'Original ARTIST 8225BD28 vs native compiled PreScene; poisoned full ReplayDirector state and ordered external calls. Attribute constructors/collections, camera and tracker calls, Fill and virtual behaviour/policy calls controlled. Actual original and native parameter setters execute; parameter debug-name pointers normalized. Valid asset data only; zero-key assertion line/order included, other assertion paths and full providers excluded.'};(O/'replay-prescene-comparison-setters-keyassert.json').write_text(json.dumps(r,indent=2)+'\n');print(json.dumps(r,indent=2))






