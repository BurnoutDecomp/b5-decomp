"""Complete native joining against ARTIST, including record publication and wrap."""
import hashlib,json,random,struct
from pathlib import Path
import native_atomic_fixture as F
ROOT=Path(__file__).resolve().parents[2]/'b5-decomp'
OUT=ROOT/'build/x360/native-thread-join-final'
row=next(r for r in json.loads((ROOT/'build/x360/native-thread-join-complete/compile-results.json').read_text())['results'] if r['source'].endswith('BrnEAThreadX360.cpp'))
path=Path(row['object']);bundle=F.Bundle([F.Coff(path,'?WaitForEnd@Thread@')]);bundle.select('?WaitForEnd@Thread@')
OBJ=F.M.S.SELF;AUX=F.M.S.AUX;DATA=AUX+0x1000;ALT=AUX+0x2000;OUTPUT=AUX+0x3000
rng=random.Random(360_1030_9);errors=[]
class Stop(Exception):pass
for index in range(384):
    mode=index%6;base=[0,0xFFFFD8EF,0xFFFFFF00,0x12345678][(index//6)%4]
    deadline=[0,0xFFFFFFFF,(base+10003)&0xFFFFFFFF,(base+9999)&0xFFFFFFFF][(index//24)%4]
    status=[0,0x102,0xFFFFFFFF,1,0x80000000,rng.getrandbits(32)][(index//4)%6]
    image=bytearray(rng.randbytes(0x100));data=bytearray(rng.randbytes(0x4000))
    struct.pack_into('>I',image,0,DATA if mode>=3 else 0)
    struct.pack_into('>I',data,0x1000,0 if mode==3 else 0x12345678)
    struct.pack_into('>I',data,0x2000,0x87654320)
    struct.pack_into('>I',data,0x3000,0xCAFEBABE)
    both=[]
    for compiled in [False,True]:
        c=F.cpu(bundle);c.mem.wr(OBJ,image);c.mem.wr(AUX,data);F.put(c,AUX,deadline)
        trace=[];state={'ticks':0,'sleeps':0}
        def tick(x):
            value=(base+state['ticks'])&0xFFFFFFFF;state['ticks']+=1
            trace.append(('tick',value));x.setg(3,value)
            if mode==1 and state['ticks']==1:F.put(x,OBJ,DATA)
            if index%17==0:F.put(x,AUX,(value+10001)&0xFFFFFFFF)
        def sleep(x):
            trace.append(('sleep',x.g(3)&0xFFFFFFFF,x.g(4)&0xFFFFFFFF));state['sleeps']+=1
            if mode==2:
                F.put(x,OBJ,DATA);F.put(x,DATA,0 if state['sleeps']==1 else 0x12345678)
            if state['sleeps']==3:raise Stop()
            x.setg(3,0xC0)
        def wait(x):
            trace.append(('wait',x.g(3)&0xFFFFFFFF,x.g(4)&0xFFFFFFFF))
            if index%13==0:F.put(x,OBJ,ALT)
            x.setg(3,status)
        def close(x):trace.append(('close',x.g(3)&0xFFFFFFFF));x.setg(3,1)
        callbacks={'GetTickCount':tick,'SleepEx':sleep,'WaitForSingleObject':wait,'CloseHandle':close}
        for at,name in bundle.unresolved.items():
            if name in callbacks:c.hooks[at]=callbacks[name]
        for at,fn in [(0x82929AA8,tick),(0x82929AE0,sleep),(0x8292C808,wait),(0x829278B8,close)]:c.hooks[at]=fn
        c.setg(3,OBJ);c.setg(4,AUX);c.setg(5,OUTPUT if index%2 else 0)
        stopped=False
        try:c.run(bundle.entry if compiled else 0x82B42DD0)
        except Stop:stopped=True
        both.append((None if stopped else c.g(3)&0xFFFFFFFF,c.mem.rd(OBJ,0x100),c.mem.rd(AUX,0x4000),trace,c.msr,c.fence_trace,stopped))
    if both[0]!=both[1]:errors.append({'index':index,'mode':mode,'return':[o[0] for o in both],
                                     'trace':[o[3] for o in both],'fences':[o[5] for o in both],
                                     'bytes':[(i,a,b) for i,(a,b) in enumerate(zip(both[0][2],both[1][2])) if a!=b][:12]})
report={'cases':384,'mismatches':errors,'object':str(path),'object_sha256':hashlib.sha256(path.read_bytes()).hexdigest(),
        'scope':'whole actual native WaitForEnd vs ARTIST; native deadline/output ABI,pre-start publication,present null handle,biased clock wrap,timeout and random OS status mapping,record replacement at wait boundary,deadline changes at tick boundary,whole memory and lwsync state witnesses',
        'limits':'kernel services controlled;unbounded pre-start wait cut after3SleepEx calls;no hardware scheduling'}
(OUT/'join-comparison.json').write_text(json.dumps(report,indent=2)+'\n')
print('join384 checks;',len(errors),'mismatches')
if errors:print(json.dumps(errors[:3],indent=2))
raise SystemExit(bool(errors))
