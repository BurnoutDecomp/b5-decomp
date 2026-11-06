"""Review regression: withdraw/replace the thread record at native publication fences."""
import hashlib,json,random,struct
from pathlib import Path
import native_atomic_fixture as F
ROOT=Path(__file__).resolve().parents[2]/'b5-decomp';OUT=ROOT/'build/x360/native-thread-join-final'
override=ROOT/'build/x360/native-thread-join-recheck2/compile-results.json'
report=override if override.exists() else OUT/'compile-results.json'
row=next(r for r in json.loads(report.read_text())['results'] if r['source'].endswith('BrnEAThreadX360.cpp'))
path=Path(row['object']);bundle=F.Bundle([F.Coff(path,'?WaitForEnd@Thread@')]);bundle.select('?WaitForEnd@Thread@')
OBJ=F.M.S.SELF;AUX=F.M.S.AUX;DATA=AUX+0x100;ALT=AUX+0x200;OUTPUT=AUX+0x300
rng=random.Random(360_1030_13);errors=[]
class Fault(Exception):pass
for index in range(128):
    mode=index%4;image=bytearray(rng.randbytes(0x100));data=bytearray(rng.randbytes(0x400))
    struct.pack_into('>I',image,0,DATA if mode==3 else 0)
    struct.pack_into('>I',data,0x100,0x12345678);struct.pack_into('>I',data,0x200,0 if index%5==0 and mode!=1 else 0x87654320)
    deadline=11000+index;both=[]
    for compiled in [False,True]:
        c=F.cpu(bundle);c.mem.wr(OBJ,image);c.mem.wr(AUX,data);F.put(c,AUX,deadline)
        trace=[];state={'fences':0,'sleeps':0,'withdrawn':False}
        def observe(pc,word):
            if word==0x7c2004ac:
                state['fences']+=1
                if state['fences']==(1 if mode==3 else 3):
                    value=ALT if mode==2 else 0;F.put(c,OBJ,value);state['withdrawn']=True
                    trace.append(('publication-change',value))
        c.instruction_observer=observe
        def tick(x):
            value=deadline-10000 if state['withdrawn'] and mode==0 else 100
            trace.append(('tick',value));x.setg(3,value)
        def sleep(x):
            state['sleeps']+=1;trace.append(('sleep',x.g(3)&0xffffffff,x.g(4)&0xffffffff))
            assert state['sleeps']<=3,(index,compiled,mode,state,trace)
            F.put(x,OBJ,ALT if mode==1 and state['withdrawn'] else DATA);x.setg(3,0)
        def wait(x):trace.append(('wait',x.g(3)&0xffffffff,x.g(4)&0xffffffff));x.setg(3,[0,258,0xffffffff][index%3])
        def close(x):trace.append(('close',x.g(3)&0xffffffff));x.setg(3,1)
        services={'GetTickCount':tick,'SleepEx':sleep,'WaitForSingleObject':wait,'CloseHandle':close}
        for at,name in bundle.unresolved.items():
            if name in services:c.hooks[at]=services[name]
        for at,fn in [(0x82929aa8,tick),(0x82929ae0,sleep),(0x8292c808,wait),(0x829278b8,close)]:c.hooks[at]=fn
        read=c.mem.rd
        def guarded_read(at,size):
            if at<0x1000:trace.append(('null-dereference',at,size));raise Fault()
            return read(at,size)
        c.mem.rd=guarded_read;c.setg(3,OBJ);c.setg(4,AUX);c.setg(5,OUTPUT if index%2 else 0)
        fault=False
        try:c.run(bundle.entry if compiled else 0x82b42dd0)
        except Fault:fault=True
        c.mem.rd=read
        both.append((fault,None if fault else c.g(3)&0xffffffff,c.mem.rd(OBJ,0x100),c.mem.rd(AUX,0x400),trace,c.fence_trace))
    if both[0]!=both[1]:errors.append({'index':index,'mode':mode,'fault':[b[0] for b in both],'return':[b[1] for b in both],'trace':[b[4] for b in both]})
d={'cases':128,'mismatches':errors,'object':str(path),'object_sha256':hashlib.sha256(path.read_bytes()).hexdigest(),
   'scope':'complete actual native join against ARTIST with record withdrawal/replacement at publication fence and recovery by timeout or later publication; initially present record withdrawal at first fence; null memory traps,whole object/buffer,callbacks and fence state',
   'limits':'deterministic instruction-boundary interference and controlled kernel callbacks;no physical concurrent thread execution'}
(OUT/'join-publication-comparison.json').write_text(json.dumps(d,indent=2)+'\n')
print('publication128 checks;',len(errors),'mismatches')
if errors:print(json.dumps(errors[:2],indent=2))
raise SystemExit(bool(errors))
