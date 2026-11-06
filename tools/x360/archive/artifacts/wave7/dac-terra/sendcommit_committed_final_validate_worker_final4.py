import sys,json,random,struct,hashlib
from pathlib import Path
root=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(root/'scratch/x360_sdk'));import native_atomic_fixture as F
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave7/dac-terra');report=json.loads((out/'send-committed/compile-results.json').read_text());p=Path(next(x['object'] for x in report['results'] if x['source'].endswith('/Dac.cpp')));b=F.Bundle([F.Coff(p,'?XenonThread@Dac@')]);b.select('?XenonThread@Dac@');selfaddr=F.M.S.SELF;aux=F.M.S.AUX;errors=[];rng=random.Random(0x96f40)
names={'lock':'?ExecuteCommandsLock@System@','wait':'XAudioWaitForSingleObject','start':'?Start@Profiler@','cycle':'?GetCpuCycle@','commands':'?ExecuteCommands@System@','copy':'XMemCpy','set':'XMemSet','voice':'XAudioVoice_GetVoiceState','submit':'XAudioSourceVoice_SubmitPacket','end':'?End@Profiler@','unlock':'?ExecuteCommandsUnlock@System@'}
orig={'lock':0x82b6bca0,'wait':0x82c2d540,'start':0x82b6a9c8,'cycle':0x82b64558,'commands':0x82b6f6d0,'copy':0x82926ba0,'set':0x82926fd0,'voice':0x82c2d030,'submit':0x82c2d088,'end':0x82b6a9f8,'unlock':0x82b6bcb0,'mix':0x82b96e80}
assert F.M.V.x360rd.u32(0x82815570)==0x7c8a2378
for i in range(96):
 img=bytearray(rng.randbytes(0x3200));struct.pack_into('>I',img,4,aux+0x100);struct.pack_into('>I',img,0x40,aux+0x200);struct.pack_into('>III',img,0x3044,0,0,aux+0x300);struct.pack_into('>I',img,0x3050,aux+0x1000);struct.pack_into('>I',img,0x30a8,aux+0x3000);struct.pack_into('>II',img,0x3100,0,(i//8)%2)
 pcm=rng.randbytes(0x1800);started=i%2;mixok=(i//2)%2;ready=(i//4)%2;quitcommands=(i//16)%3==1;stopcommands=(i//16)%3==2;both=[]
 for compiled in (False,True):
  c=F.cpu(b);c.mem.add(0x8327a000,0x5000);c.mem.wr(selfaddr,img);c.mem.wr(aux+0x1000,bytes(0x4000));c.setg(3,selfaddr);trace=[];state={'waits':0,'cycles':10};
  running=F.address(b,'sThreadRunning') if compiled else 0x8327a588;startedaddr=F.address(b,'sStarted') if compiled else 0x8327a587;prof=F.address(b,'spProfiler') if compiled else 0x8327a580;bal=F.address(b,'sCpuLoadBalancer') if compiled else 0x8327ee6c;inter=F.address(b,'sInterleaveBuffer') if compiled else 0x8327d600
  c.mem.wr(running,b'\x01');c.mem.wr(startedaddr,bytes([started]));F.put(c,prof,aux+0x500);F.put(c,bal+4,0x1234);F.put(c,bal+8,0);c.mem.wr(inter,pcm)
  def make(k):
   def call(x):
    if k=='wait':
     state['waits']+=1;trace.append((k,x.g(3)&0xffffffff,x.g(4)&0xffffffff,x.g(5)&0xffffffff));
     if state['waits']==2:x.mem.wr(running,b'\0')
    elif k=='commands':
     trace.append((k,x.g(3)&0xffffffff));
     if quitcommands:x.mem.wr(running,b'\0')
     if stopcommands:x.mem.wr(startedaddr,b'\0')
    elif k=='cycle':state['cycles']+=7;x.setg(3,state['cycles']);trace.append((k,))
    elif k=='mix':trace.append((k,x.g(3)&0xffffffff));x.setg(3,mixok)
    elif k=='copy':
     dest,src,n=[x.g(z)&0xffffffff for z in (3,4,5)];trace.append((k,dest,n));x.mem.wr(dest,x.mem.rd(src,n))
    elif k=='set':
     dest,val,n=[x.g(z)&0xffffffff for z in (3,4,5)];trace.append((k,dest,val,n));x.mem.wr(dest,bytes([val&255])*n)
    elif k=='voice':trace.append((k,x.g(3)&0xffffffff));x.mem.wr(x.g(4)&0xffffffff,bytes([0x20 if ready else 0]))
    elif k=='submit':trace.append((k,x.g(3)&0xffffffff,(x.g(4)&0xffffffff)-selfaddr,x.g(5)&0xffffffff))
    else:trace.append((k,x.g(3)&0xffffffff))
   return call
  for k,addr in orig.items():c.hooks[addr]=make(k)
  for addr,name in b.unresolved.items():
   for k,prefix in names.items():
    if name.startswith(prefix):c.hooks[addr]=make(k)
  c.hooks[F.address(b,'?Mix@Dac@')]=make('mix')
  c.run(b.entry if compiled else 0x82b96f40);both.append((c.mem.rd(selfaddr,0x3200).hex(),c.mem.rd(aux+0x1000,0x4000).hex(),c.mem.rd(bal+4,8).hex(),c.mem.rd(startedaddr,1).hex(),trace))
 if both[0]!=both[1]:errors.append({'case':i,'original_trace':both[0][4],'compiled_trace':both[1][4],'original_bal':both[0][2],'compiled_bal':both[1][2]})
r={'cases':96,'mismatches':errors,'original':'82B96F40','object':str(p),'sha256':hashlib.sha256(p.read_bytes()).hexdigest(),'limits':'Actual compiled worker compared to ARTIST PPC with controlled System, Profiler, Mix, XAudio, GetCpuCycle and XMem boundaries; traces, DAC/packet memory and load accounting compared. Sequential synthetic runs only, no actual kernel or thread concurrency, mixer, physical device, full image or boot.'};(out/'worker-final4-comparison-abi-final.json').write_text(json.dumps(r,indent=2)+'\n');print('cases',96,'mismatches',len(errors));print(errors[:1]);assert not errors
