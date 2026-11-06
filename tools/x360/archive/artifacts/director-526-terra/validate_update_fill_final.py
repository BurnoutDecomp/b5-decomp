from pathlib import Path
import json,random,struct,sys,hashlib
P=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(P/'scratch/x360_sdk'));import native_atomic_fixture as F
O=Path('D:/Reverse/BP-X360-Experimental-Artifacts/director-526-terra');j=json.loads((O/'fill-final/compile-results.json').read_text());obj=next(Path(x['object']) for x in j['results'] if x['source'].endswith('BrnReplayDirector.cpp'));sym='?Update@ReplayDirector@BrnDirector@@';b=F.Bundle([F.Coff(obj,sym)]);b.select(sym)
S=F.M.S.SELF;A=F.M.S.AUX;IO=A+0x1000;IN=A+0x2000;OUT=A+0x3000;FRAME=A+0x4000;USED=A+0x5000;CTRL=A+0x6000;VT=A+0x7000;POL=A+0x8000;PVT=A+0x9000;OUTCAM=A+0xa000;OUTCGS=A+0xb000; VGET=0x84f00000;VPOST=0x84f00010;VPOL=0x84f00020
orig=json.load(open('D:/Reverse/BP-Decomp_Workflow/.ida-exports/BURNOUT_X360_ARTIST.XEX/0x8225C298.json'))['assembly'].splitlines()
def target(pc):
 w=F.M.V.x360rd.u32(pc);d=w&0x03fffffc;d=d-0x04000000 if d&0x02000000 else d;return (pc+d)&0xffffffff
def put(c,a,x):c.mem.wr(a,struct.pack('>I',x&0xffffffff))
def effects(c,n,t,player,mask):
 if 'GetPlayerCarIndex' in n:c.setg(3,player)
 elif 'GetUsedRaceCars' in n:c.setg(3,USED)
 elif 'FillInSharedInfo' in n:
  assert c.g(6)==FRAME and c.g(7)==IO;t.append('fill')
 elif 'GetControll' in n:c.setg(3,CTRL);t.append('control')
 elif 'Camera__Camera__Construct' in n or '?Construct@Camera@' in n:
  c.mem.wr(c.g(3),bytes([0xa5])*0x160);t.append('construct')
 elif 'Camera__Camera__operator_' in n or ('??4Camera@' in n and 'BrnDirector' in n):
  dst,src=c.g(3),c.g(4);c.mem.wr(dst,c.mem.rd(src,0x15e));t.append(('copy',src-S if S<=src<S+0x29e0 else ('frame' if src==FRAME else 'temp')));c.setg(3,dst)
 elif 'CopyToCgsCamera' in n:
  c.mem.wr(c.g(4),c.mem.rd(c.g(3),0x160)+b'\0'*0x10);t.append('copy_cgs')
 elif 'SetCameraOutput' in n:c.mem.wr(OUTCAM,c.mem.rd(c.g(4),0x160));t.append('output_camera')
 elif 'SetCgsCamera' in n:c.mem.wr(OUTCGS,c.mem.rd(c.g(4),0x170));t.append('output_cgs')
 elif any(x in n for x in ['BeginAssert','FireAssert','EndAssert']):raise AssertionError(('assert',n))
 else:raise AssertionError(('unhandled',n))
class ReplayCPU(F.AtomicCPU):
 def step(self,pc,w,nxt,depth):
  if w>>26==31 and ((w>>1)&1023)==27: # sld
   rs=(w>>21)&31;ra=(w>>16)&31;rb=(w>>11)&31; shift=self.g(rb)&127
   self.setg(ra,(self.g(rs)<<shift)&0xffffffffffffffff if shift<64 else 0)
   return nxt
  return super().step(pc,w,nxt,depth)

def run_one(compiled,initial,frame,player,used,cycle,mask):
 c=F.cpu(b);c.__class__=ReplayCPU;c.mem.wr(S,initial);c.mem.wr(FRAME,frame);c.mem.wr(IO,struct.pack('>5I',IN,OUT,0,0,0));c.mem.wr(USED,(1<<player if used and player>=0 else 0).to_bytes(8,'big'));c.mem.wr(CTRL,b'\0\0\0'+bytes([cycle]));put(c,S+0x690,VT);put(c,S+0x1200,VT);put(c,VT+0x14,VGET);put(c,VT+0x0c,VPOST);put(c,POL,PVT);put(c,PVT+4,VPOL);c.mem.wr(OUTCAM,b'\x5a'*0x160);c.mem.wr(OUTCGS,b'\x6b'*0x170);t=[]
 def getpolicy(cc):
  index=0 if cc.g(3)==S+0x690 else 1;t.append(('getpolicy',index));cc.setg(3,POL if (mask>>index)&1 else 0)
 def post(cc):t.append(('post',cc.g(3)-S,cc.g(4)-S))
 def policy(cc):t.append(('policy',cc.g(5)-S))
 c.hooks.update({VGET:getpolicy,VPOST:post,VPOL:policy})
 for k,v in [(3,S),(4,FRAME),(5,IO)]:c.setg(k,v)
 if compiled:
  for a,n in b.unresolved.items():c.hooks[a]=lambda cc,name=n:effects(cc,name,t,player,mask)
  for n,(oi,v,sec) in b.definitions.items():
   if (oi,sec) in b.bases and (('??4Camera@' in n and 'BrnDirector' in n) or '?FillInSharedInfo@ReplayDirector' in n):c.hooks[b.bases[(oi,sec)]+v]=lambda cc,name=n:effects(cc,name,t,player,mask)
  c.run(b.entry)
 else:
  for l in orig:
   if '  bl ' not in l:continue
   p=l.split();n=p[2]
   if n.startswith('__save') or n.startswith('__rest'):continue
   c.hooks[target(int(p[0],16))]=lambda cc,name=n:effects(cc,name,t,player,mask)
  c.run(0x8225c298)
 return (c.mem.rd(S,0x29e0),c.mem.rd(OUTCAM,0x160),c.mem.rd(OUTCGS,0x170),t)
rng=random.Random(0x8225c298);count=0
for flags in (0,8,0x400,0x408):
 for reset in (0,1):
  for selected in (0,1,-1,2,0x7fffffff):
   for cycle in (0,1):
    for mask in range(4):
     initial=bytearray(rng.randbytes(0x29e0));putting=struct.pack('>I',selected & 0xffffffff);initial[0x29d0:0x29d4]=putting;initial[0x29d4]=reset;frame=bytearray(rng.randbytes(0x180));frame[0x140:0x148]=flags.to_bytes(8,'big');
     a=run_one(False,bytes(initial),bytes(frame),2,True,cycle,mask);z=run_one(True,bytes(initial),bytes(frame),2,True,cycle,mask)
     if a!=z: raise AssertionError((flags,reset,selected,cycle,mask,a[3],z[3], [hex(k) for k,(x,y) in enumerate(zip(a[0],z[0])) if x!=y][:16]))
     count+=1
for player,used in [(-1,False),(3,False)]:
 for i in range(16):
  initial=rng.randbytes(0x29e0);frame=rng.randbytes(0x180);a=run_one(False,initial,frame,player,used,0,0);z=run_one(True,initial,frame,player,used,0,0)
  if a!=z:raise AssertionError(('early',player,i,a[3],z[3]))
  count+=1
r={'cases':count,'object':str(obj),'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'Original8225C298 vs actual compiled ReplayDirector::Update. Tests all combinations of gameplay/crash flags, reset, camera selection, cycle button, two optional collision policies, plus no-player/unused early outs; compare full 0x29E0 ReplayDirector memory, both output camera images and ordered service/virtual call traces. FillInSharedInfo, camera construction/copy/conversion, output setters and collision/behaviour calls are controlled mocks; invalid player assert paths and real gameplay are not covered.'};(O/'replay-update-comparison-fill-final.json').write_text(json.dumps(r,indent=2)+'\n');print(json.dumps(r,indent=2))


