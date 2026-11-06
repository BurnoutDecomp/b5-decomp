import sys,json,random,struct,hashlib
from pathlib import Path
root=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(root/'scratch/x360_sdk'));import native_atomic_fixture as F
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave7/dac-terra');report=json.loads((out/'mixer-layout/compile-results.json').read_text());p=Path(report['results'][0]['object']);mixerpath=Path(next(x['object'] for x in report['results'] if x['source'].endswith('/Mixer.cpp')));b=F.Bundle([F.Coff(p,'?DacCreateInstanceThunk@'),F.Coff(mixerpath,'?Mixer_ctor@Mixer@')]);b.select('?DacCreateInstanceThunk@');selfaddr=F.M.S.SELF;aux=F.M.S.AUX;mix=0x50000000;errors=[];rng=random.Random(0x24a0)
orig={'memset':0x82c0b180,'balance':0x82b6c6f0,'profiler':0x82b6de08,'alloc':0x82b6be18,'init':0x82c2d3c0,'voice':0x82c2d3c8,'xset':0x82926fd0,'event':0x82c2d488,'threadctor':0x82b42dc0,'params':0x82b42d58,'wrapper':0x82b42d88,'begin':0x82b43f80,'threadid':0x82b6bcb8,'threadend':0x82b43e88,'start':0x82c2d0e8}
names={'memset':'memset','balance':'?Init@CpuLoadBalancer@','profiler':'?GetProfiler@System@','alloc':'?Alloc@System@','init':'XAudioInitialize','voice':'XAudioCreateSourceVoice','xset':'XMemSet','event':'XAudioCreateEvent','threadctor':'??0Thread@','params':'??0ThreadParameters@','wrapper':'?GetGlobalRunnableFunctionUserWrapper@','begin':'?Begin@Thread@','threadid':'?SetRwAudioCoreThreadId@','threadend':'??1Thread@','start':'XAudioSourceVoice_Start'}
fxorig={0x829643c8:'qsource',0x829643f0:'csource',0x82964088:'qrouter',0x829640d0:'crouter',0x82963070:'qrender',0x829630f8:'crender',0x829610b8:'qreverb',0x82962cc0:'creverb'}
fxnames={'XAudioQuerySourceEffectSize':'qsource','XAudioCreateSourceEffect':'csource','XAudioQueryRouterEffectSize':'qrouter','XAudioCreateRouterEffect':'crouter','XAudioQueryNativeRenderEffectSize':'qrender','XAudioCreateNativeRenderEffect':'crender','XAudioQueryReverbEffectSize':'qreverb','XAudioCreateReverbEffect':'creverb'}
assert F.M.V.x360rd.u32(0x82815570)==0x7c8a2378
for i in range(48):
 initial=bytearray(rng.randbytes(0x3200));system=bytearray(rng.randbytes(0x1200));struct.pack_into('>I',initial,4,aux+0x1000);struct.pack_into('>I',system,0,aux+0x8000);struct.pack_into('>II',system,0x10ec,17,0x8000);system[0x10fa]=4;failprof=i%6==0;failalloc=i%6==1;external=i%6==2;both=[]
 for compiled in (False,True):
  c=F.cpu(b);c.mem.add(0x8327a000,0x5000);c.mem.add(0x83270000,0x8000);c.mem.add(mix,0x31000);c.mem.wr(selfaddr,initial);c.mem.wr(aux+0x1000,system);c.setg(3,selfaddr);trace=[];F.put(c,0x83271928,aux+0x1000);
  for at,name in b.unresolved.items():
   if name=='off_83271928':F.put(c,at,aux+0x1000)
  ext=F.address(b,'sExternalDacMode') if compiled else 0x8327ee58;F.put(c,ext,int(external));
  symbol_by_addr={a:fxnames.get(n,n) for a,n in b.unresolved.items()};symbol_by_addr.update(fxorig)
  for addr in [F.address(b,'?XenonProcessCb@Dac@'),0x82b97208]:symbol_by_addr[addr]='process'
  for addr in [F.address(b,'?XenonPacketCompleteCb@Dac@'),0x82b97228]:symbol_by_addr[addr]='complete'
  for addr in [F.address(b,'?XenonThreadFunc@Dac@'),0x82b97150]:symbol_by_addr[addr]='threadfunc'
  def word(x,a):return int.from_bytes(x.mem.rd(a,4),'big')
  def make(k):
   def call(x):
    a=[x.g(z)&0xffffffff for z in range(3,8)]
    if k in ('memset','xset'):
     x.mem.wr(a[0],bytes([a[1]&255])*a[2]);x.setg(3,a[0]);
     if k=='xset':trace.append((k,a[0]-selfaddr,a[1],a[2]))
    elif k=='balance':trace.append((k,a[1]));x.setg(3,a[0])
    elif k=='profiler':trace.append((k,a[0]));x.setg(3,0 if failprof else aux+0x900)
    elif k=='alloc':trace.append((k,*a));x.setg(3,0 if failalloc else mix)
    elif k=='mixer':trace.append((k,a[0]));x.setg(3,a[0])
    elif k=='init':
     table=word(x,a[0]+4);count=x.mem.rd(table,1)[0];entries=word(x,table+4);es=[]
     for j in range(count):
      e=entries+j*12;es.append((x.mem.rd(e,1)[0],symbol_by_addr.get(word(x,e+4),'?'),symbol_by_addr.get(word(x,e+8),'?')))
     trace.append((k,count,tuple(es)))
    elif k=='voice':
     buf=bytearray(x.mem.rd(a[0],0x5c));buf[1:4]=b'\0'*3
     for offset in (0x4c,0x50,0x54):
      v=struct.unpack_from('>I',buf,offset)[0];struct.pack_into('>I',buf,offset,{'process':1,'complete':2}.get(symbol_by_addr.get(v),v))
     trace.append((k,bytes(buf).hex(),a[1]-selfaddr));F.put(x,a[1],aux+0x800)
    elif k=='event':trace.append((k,a[0],a[1]));x.setg(3,aux+0x700)
    elif k=='threadctor':F.put(x,a[0],0)
    elif k=='params':
     for j in (0,4,8,12,16,20):F.put(x,a[0]+j,0)
    elif k=='wrapper':x.setg(3,0)
    elif k=='begin':
     ps=a[3];nameptr=word(x,ps+20);trace.append((k,symbol_by_addr.get(a[1],'?'),a[2],word(x,ps+4),word(x,ps+8),word(x,ps+12),x.mem.rd(ps+16,1)[0],F.cstring(x,nameptr),a[4]));x.setg(3,aux+0x600)
    elif k=='threadid':trace.append((k,a[0],word(x,a[1])))
    elif k=='threadend':trace.append((k,))
    elif k=='start':trace.append((k,a[0],a[1]))
   return call
  for k,addr in orig.items():c.hooks[addr]=make(k)
  for addr,name in b.unresolved.items():
   for k,prefix in names.items():
    if name.startswith(prefix):c.hooks[addr]=make(k)
  c.run(b.entry if compiled else 0x82ba24a0)
  obj=bytearray(c.mem.rd(selfaddr,0x3200));obj[:4]=bytes(4)
  scratch=F.address(b,'sMixerScratchArena') if compiled else 0x83271d00
  if not failprof and not failalloc:
   for off in (4,8,12):F.put(c,aux+0x8000+off,(int.from_bytes(c.mem.rd(aux+0x8000+off,4),'big')-scratch)&0xffffffff)
  both.append((obj.hex(),c.mem.rd(aux+0x1000,0x1200).hex(),c.mem.rd(mix,0x31000).hex(),c.mem.rd(aux+0x8000,16).hex(),trace,c.g(3)&0xffffffff))
 if both[0]!=both[1]:errors.append({'case':i,'original_trace':both[0][4],'compiled_trace':both[1][4],'ret':[both[0][5],both[1][5]],'stack':[both[0][3],both[1][3]],'obj_diff':[hex(j) for j,(x,y) in enumerate(zip(bytes.fromhex(both[0][0]),bytes.fromhex(both[1][0]))) if x!=y][:32]})
r={'cases':48,'mismatches':errors,'original':'82BA24A0','object':str(p),'sha256':hashlib.sha256(p.read_bytes()).hexdigest(),'limits':'Actual native descriptor thunk + CreateInstance versus original with vtable and scratch arena pointers normalized, genuine effect-table entries and voice init inspected, actual original and compiled Mixer constructors executed, System, XAudio, XMem and EAThread calls controlled. DAC, System, Mixer bytes and service traces compared. No actual SDK calls, kernel concurrency, hardware, link or boot claim.'};(out/'create-mixer-composed-comparison.json').write_text(json.dumps(r,indent=2)+'\n');print('cases',48,'mismatches',len(errors));print(errors[:1]);assert not errors
