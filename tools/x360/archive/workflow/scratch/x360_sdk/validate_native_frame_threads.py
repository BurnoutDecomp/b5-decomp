import os
"""Execute native frame lifecycle/callback ordering against complete ARTIST functions."""
import hashlib,json,random,struct
from pathlib import Path
import native_atomic_fixture as F
ROOT=Path(__file__).resolve().parents[2]/'b5-decomp'
OUT=ROOT/'build/x360/native-frame-final'
REPORT=Path(os.environ.get('NATIVE_FRAME_REPORT',str(OUT/'compile-results.json')))
rows=json.loads(REPORT.read_text())['results']
path=Path(next(r['object'] for r in rows if r['source'].endswith('CgsThreadLayout.cpp')))
bundle=F.Bundle([F.Coff(path,'?Update@ThreadLayout@')])
OBJ=F.M.S.SELF;AUX=F.M.S.AUX;CALLBACK=AUX+0x1000;VTABLE=AUX+0x1100;STREAM=AUX+0x2000
identity=json.loads((ROOT.parent/'progress/identity.json').read_text())
rng=random.Random(360_1030_6)
errors=[];groups=[]

def global_at(hi,lo):
    high=F.M.V.x360rd.u32(hi)&0xFFFF;low=F.M.V.x360rd.u32(lo)&0xFFFF
    if low&0x8000:low-=0x10000
    return ((high<<16)+low)&0xFFFFFFFF

OLD_FILTER=global_at(0x828E170C,0x828E1714)
OLD_STREAM=global_at(0x828E1710,0x828E172C)

class Stop(Exception):pass

specs=[('??0ThreadLayout@',0x828E15C8,'ctor',64),
       ('?InitThreads@ThreadLayout@',0x828E1658,'init',96),
       ('?Update@ThreadLayout@',0x828E17D0,'update',256),
       ('?InteruptThreadForAssert@ThreadLayout@',0x828D79D8,'assert',192),
       ('?DispatchThread@ThreadLayout@',0x828D7988,'dispatch',32),
       ('?Begin@ThreadLayout@',0x828EBAA0,'begin',64),
       ('?GlobalThreadBeginWrapper@ThreadLayout@',0x828D7978,'wrapper',32)]
for prefix,original,kind,count in specs:
    bundle.select(prefix)
    def loaded(fragment):return F.address(bundle,fragment)
    for index in range(count):
        image=bytearray(rng.randbytes(0x100))
        struct.pack_into('>IiiiI',image,0,CALLBACK,[-1,0,3][index%3],[-1,0,2][(index//3)%3],[-1,0,1][(index//9)%3],0x13572468)
        image[0x78]=index%2
        struct.pack_into('>I',image,0x7C,AUX+0x4000 if kind=='update' and index%13==0 else 0)
        both=[]
        for compiled in [False,True]:
            c=F.cpu(bundle);c.failures=index%3;c.mem.wr(OBJ,image)
            pages={OLD_FILTER&~0xFFF,OLD_STREAM&~0xFFF,0x830EA000}
            for page in pages:c.mem.add(page,0x2000 if page==0x830EA000 else 0x1000)
            c.mem.wr(CALLBACK,VTABLE.to_bytes(4,'big'))
            for slot in range(7):F.put(c,VTABLE+slot*4,AUX+0x1800+slot*16)
            for offset in [0,0x40,0x80]:F.put(c,STREAM+offset,AUX+0x2100)
            F.put(c,AUX+0x2104,AUX+0x2200)
            trace=[];state={'barriers':0,'sleeps':0,'ticks':0}
            filter_value=[0,1,0x100000000,0x100000001][index%4]
            for at,name in bundle.unresolved.items():
                if 'gxMessageFilterFlags' in name:F.put(c,at,filter_value,8)
                elif 'gpDebugPrint' in name:F.put(c,at,STREAM)
            F.put(c,OLD_FILTER,filter_value,8);F.put(c,OLD_STREAM,STREAM)
            def set_filter(value):
                F.put(c,OLD_FILTER,value,8)
                for at,name in bundle.unresolved.items():
                    if 'gxMessageFilterFlags' in name:F.put(c,at,value,8)
            debug_on=kind=='update' and index%4==0
            park_stall=debug_on and index%32==0
            continue_value=int(not park_stall)
            c.mem.wr(0x830EA8B5,bytes([int(debug_on)]));c.mem.wr(0x830EA8B4,bytes([continue_value]))
            if kind=='update':
                c.mem.wr(loaded('_lbAllowStallTest'),bytes([int(debug_on)]))
                c.mem.wr(loaded('_lbContinue'),bytes([continue_value]))
            read_state={'reads':0,'steps':None}
            original_read=c.mem.rd
            if park_stall:
                flag_address=loaded('_lbContinue') if compiled else 0x830EA8B4
                def observe_read(at,size):
                    if at==flag_address and size==1:
                        read_state['reads']+=1
                        if read_state['steps'] is None:read_state['steps']=0
                    return original_read(at,size)
                def observe_instruction(pc,word):
                    if read_state['steps'] is not None:
                        read_state['steps']+=1
                        if read_state['steps']==40:raise Stop()
                c.mem.rd=observe_read;c.instruction_observer=observe_instruction
            def callback(slot):
                def called(x):
                    trace.append(('callback',slot,x.g(3)&0xFFFFFFFF,(x.g(4)&0xFFFFFFFF if slot in [2,6] else None),x.mem.rd(OBJ+0x78,1)[0]))
                    if slot==0:x.setg(3,index%2)
                return called
            for slot in range(7):c.hooks[AUX+0x1800+slot*16]=callback(slot)
            def barrierwait(x):
                state['barriers']+=1
                trace.append(('barrier',x.g(3)&0xFFFFFFFF,x.rd32(x.g(4)&0xFFFFFFFF),x.mem.rd(OBJ+0x78,1)[0]))
                if kind=='dispatch' and state['barriers']==7:raise Stop()
                x.setg(3,0)
            def gettime(x):
                now=[0xFFFFFF00,0,0x12345678][index%3]
                value=(now+(1000 if debug_on else 17)*state['ticks'])&0xFFFFFFFF
                state['ticks']+=1;trace.append(('time',value));x.setg(3,value)
            def monitor_start(x):trace.append(('perf-start',x.g(3)&0xFFFFFFFF))
            def monitor_stop(x):trace.append(('perf-stop',x.g(3)&0xFFFFFFFF))
            def printf(x):trace.append(('printf',F.cstring(x,x.g(3)&0xFFFFFFFF),x.g(4)&0xFFFFFFFF));x.setg(3,0)
            def getid(x):x.setg(3,0x24681357);trace.append(('dispatch-id',))
            def current(x):
                actor=[0x24681357,0x13572468,0xABCD][index%3]
                trace.append(('current-id',));x.setg(3,actor)
            def sleep(x):
                state['sleeps']+=1;trace.append(('sleep',x.rd32(x.g(3)&0xFFFFFFFF)))
                if state['sleeps']==2:raise Stop()
            def assert_dispatch(x):trace.append(('assert',x.g(3)&0xFFFFFFFF,x.g(4)&0xFFFFFFFF));x.setg(3,1)
            def threadctor(x):
                target=x.g(3)&0xFFFFFFFF;trace.append(('thread-ctor',target));F.put(x,target,0);x.setg(3,target)
            def barrierctor(x):
                target=x.g(3)&0xFFFFFFFF
                trace.append(('barrier-ctor',target,x.g(4)&0xFFFFFFFF,x.g(5)&0xFFFFFFFF))
                for off in [0,4,8,0xC,0x10,0x14,0x1C,0x20,0x24]:F.put(x,target+off,0)
                for off in [0x18,0x28]:x.mem.wr(target+off,b'\x01')
                x.setg(3,target)
            def barrierparams(x):
                target=x.g(3)&0xFFFFFFFF;F.put(x,target,x.g(4));x.mem.wr(target+4,bytes([x.g(5)&255,0]));x.setg(3,target)
            def barrierinit(x):
                params=x.g(4)&0xFFFFFFFF
                trace.append(('barrier-init',x.g(3)&0xFFFFFFFF,x.rd32(params),x.mem.rd(params+4,2).hex()));x.setg(3,1)
            def threadparams(x):
                target=x.g(3)&0xFFFFFFFF
                for off in [0,4,8]:F.put(x,target+off,0)
                F.put(x,target+0xC,-1);x.mem.wr(target+0x10,b'\x00');F.put(x,target+0x14,AUX+0xF00);x.mem.wr(AUX+0xF00,b'\x00');x.setg(3,target)
            def setwrapper(x):
                code=x.g(3)&0xFFFFFFFF
                assert code==(loaded('?GlobalThreadBeginWrapper@') if compiled else 0x828D7978)
                trace.append(('set-wrapper',))
            def getwrapper(x):trace.append(('get-wrapper',));x.setg(3,0x2468ACE0)
            def priority(x):trace.append(('set-priority',x.g(3)&0xFFFFFFFF));x.setg(3,1)
            def getpriority(x):trace.append(('get-priority',));x.setg(3,11)
            def dispatchpriority(x):trace.append(('get-dispatch-priority',x.g(3)&0xFFFFFFFF));x.setg(3,22)
            def begin_thread(x):
                code=x.g(4)&0xFFFFFFFF;params=x.g(6)&0xFFFFFFFF
                assert code==(loaded('?DispatchThread@') if compiled else 0x828D7988)
                trace.append(('begin-thread',x.g(3)&0xFFFFFFFF,x.g(5)&0xFFFFFFFF,
                              x.mem.rd(params,0x10).hex(),x.mem.rd(params+0x10,1)[0],
                              F.cstring(x,x.rd32(params+0x14)),x.g(7)&0xFFFFFFFF));x.setg(3,0x55)
            def text(x):
                value=F.cstring(x,x.g(4)&0xFFFFFFFF)
                trace.append(('text',x.g(3)&0xFFFFFFFF,value))
                if value=='Main thread priority: ' and index%3==0:set_filter(0)
                x.setg(3,STREAM+0x40)
            def scalar(x):trace.append(('scalar',x.g(3)&0xFFFFFFFF,x.g(4)&0xFFFFFFFF));x.setg(3,STREAM+0x80)
            def init_hook(x):trace.append(('init',x.mem.rd(OBJ,0x80).hex()))
            def runnable(x):trace.append(('runnable',x.g(3)&0xFFFFFFFF));x.setg(3,0x87654321)
            old={0x82B43168:barrierwait,0x828D75F0:gettime,0x82B42FB8:getid,0x82B42560:current,
                 0x82B42610:sleep,0x82B42DC0:threadctor,0x82B43AD8:barrierctor,
                 0x82B42650:barrierparams,0x82B430C8:barrierinit,0x82B42D58:threadparams,
                 0x82B42D98:setwrapper,0x82B42D88:getwrapper,0x82B425D8:priority,
                 0x82B425D0:getpriority,0x82B42FD8:dispatchpriority,0x82B43F80:begin_thread,
                 0x821F0E50:scalar}
            for name,fn in [('CgsDev::PerfMonCpu::StartMonitor',monitor_start),('CgsDev::PerfMonCpu::StopMonitor',monitor_stop),('printf',printf)]:
                for at in identity.get(name,{}).get('x360_addrs',[]):old[int(at,16)]=fn
            if kind=='update':old[0x828D79D8]=assert_dispatch;c.hooks[loaded('?InteruptThreadForAssert@')]=assert_dispatch
            if kind=='begin':old[0x828E1658]=init_hook;c.hooks[loaded('?InitThreads@')]=init_hook
            for at,fn in old.items():c.hooks[at]=fn
            c.hooks[AUX+0x2200]=text;c.hooks[AUX+0x5000]=runnable
            callbacks={'?Wait@Barrier@':barrierwait,'?GetSystemTimeMS@':gettime,'?GetId@Thread@':getid,
                       '?GetThreadId@':current,'?ThreadSleep@':sleep,'??0Thread@':threadctor,
                       '??0Barrier@':barrierctor,'??0BarrierParameters@':barrierparams,'?Init@Barrier@':barrierinit,
                       '??0ThreadParameters@':threadparams,'?SetGlobalRunnableFunctionUserWrapper@':setwrapper,
                       '?GetGlobalRunnableFunctionUserWrapper@':getwrapper,'?SetThreadPriority@':priority,
                       '?GetThreadPriority@':getpriority,'?GetPriority@Thread@':dispatchpriority,
                       '?Begin@Thread@':begin_thread,'?StartMonitor@':monitor_start,'?StopMonitor@':monitor_stop,
                       '??6StrStreamBase@CgsDev@@QAAA':scalar}
            for at,name in bundle.unresolved.items():
                if name=='printf':c.hooks[at]=printf
                for pattern,fn in callbacks.items():
                    if name.startswith(pattern):c.hooks[at]=fn
            c.setg(3,AUX+0x5000 if kind=='wrapper' else OBJ);c.setg(4,AUX+0x6000 if kind=='wrapper' else CALLBACK)
            c.setg(5,-1);c.setg(6,12);c.setg(7,34)
            if kind=='assert':c.setg(4,AUX+0x4000)
            stopped=False
            try:c.run(bundle.entry if compiled else original)
            except Stop:stopped=True
            c.mem.rd=original_read
            result=c.g(3)&0xFFFFFFFF if kind in ['ctor','update','assert','wrapper'] and not stopped else None
            debug=c.mem.rd((loaded('_lbContinue') if compiled else 0x830EA8B4),1) if kind=='update' else b''
            both.append((result,c.mem.rd(OBJ,0x100),trace,c.msr,c.atomic_trace,stopped,debug,read_state['reads']))
        if both[0]!=both[1]:errors.append({'kind':kind,'index':index,'return':[o[0] for o in both],
                                         'trace':[o[2] for o in both],'atomic':[o[4] for o in both],
                                         'bytes':[(i,a,b) for i,(a,b) in enumerate(zip(both[0][1],both[1][1])) if a!=b][:12]})
    print(kind,count,'checked',flush=True);groups.append({'kind':kind,'cases':count})
result={'cases':sum(g['cases'] for g in groups),'groups':groups,'mismatches':errors,
        'object':str(path),'object_sha256':hashlib.sha256(path.read_bytes()).hexdigest(),
        'scope':'complete native frame ctor/init/update/assert/begin/wrapper and bounded dispatch loop vs ARTIST; exact lifecycle and callback ordering,defined thread parameters and processor2,logging/filter reread,stall flags,performance markers,barrier deadlines,whole object and atomic stores/MSR',
        'limits':'thread,barrier,time,logging andgame callback services controlled; dispatch/secondary park loops stopped at callbacks,false debugger stall after40instructions with exact flag-read counting; no hardware concurrency or game launch'}
(OUT/'frame-comparison.json').write_text(json.dumps(result,indent=2)+'\n')
print(result['cases'],'checks;',len(errors),'mismatches')
if errors:print(json.dumps(errors[:3],indent=2))
raise SystemExit(bool(errors))
