"""Execute complete native Mutex/Condition/RWMutex functions against ARTIST."""
import ast
import hashlib
import json
from pathlib import Path
import random
import struct
import sys

sys.path.insert(0, str(Path(__file__).parent))
import validate_native_math as M
from validate_render_states import Coff, CPU
from validate_resource_pointer import Bundle

ROOT = Path(__file__).resolve().parents[2]/'b5-decomp'
OUT = ROOT/'build/x360/native-thread-sync-complete'
rows = json.loads((OUT/'compile-results.json').read_text())['results']
paths = {Path(r['source']).name:Path(r['object']) for r in rows if r['passed']}
repair = json.loads((ROOT/'build/x360/native-thread-sync-repair/compile-results.json').read_text())
paths.update({Path(r['source']).name:Path(r['object']) for r in repair['results'] if r['passed']})
tree=ast.parse((Path(__file__).parent/'validate_native_strings.py').read_text())
nodes=[n for n in tree.body if isinstance(n,(ast.ClassDef,ast.FunctionDef)) and n.name in ['StringCPU','loaded_address']]
exec(compile(ast.Module(body=nodes,type_ignores=[]),'reservation-fixture','exec'),globals())

OBJ=M.S.SELF
AUX=M.S.AUX
rng=random.Random(360_1030)
identity=json.loads((ROOT.parent/'progress/identity.json').read_text())
errors=[]
groups=[]


def wr32(c, address, value): c.mem.wr(address, struct.pack('>I',value&0xFFFFFFFF))
def s32(value): return (value&0x7FFFFFFF)-(value&0x80000000)


def fixture(bundle, now=0, attempts=0, sem_result=0, relock_error=False):
    c=bundle.cpu();c.__class__=StringCPU;c.msr=0x8000;c.reservation=None;c.failures=0
    c.mem.add(0x84800000,0x4000)
    trace=[];state={'try':0,'enters':0}
    def ptr(x,r):return x.g(r)&0xFFFFFFFF
    def enter(x):
        address=ptr(x,3);trace.append(('enter',address));state['enters']+=1
        if relock_error and address==AUX+0x1000:wr32(x,address+0x20,-2)
    def leave(x):trace.append(('leave',ptr(x,3)))
    def init(x):
        address=ptr(x,3);trace.append(('init',address));x.mem.wr(address,b'\xCD'*0x1C)
    def attempt(x):
        state['try']+=1;trace.append(('try',ptr(x,3)));x.setg(3,int(state['try']>attempts))
    def tick(x):trace.append(('tick',now));x.setg(3,now)
    def sleep(x):trace.append(('sleep',ptr(x,3)));x.setg(3,0xBB)
    def semwait(x):
        trace.append(('semwait',ptr(x,3),x.rd32(ptr(x,4))));x.setg(3,sem_result)
    def sempost(x):trace.append(('sempost',ptr(x,3),ptr(x,4)));x.setg(3,7)
    callbacks={'RtlEnterCriticalSection':enter,'RtlLeaveCriticalSection':leave,
               'RtlInitializeCriticalSection':init,'RtlTryEnterCriticalSection':attempt,
               'GetTickCount':tick,'Sleep':sleep}
    for name,fn in callbacks.items():
        for address in identity.get(name,{}).get('x360_addrs',[]):c.hooks[int(address,16)]=fn
    c.hooks[0x82B42A40]=semwait;c.hooks[0x82B42B70]=sempost
    for at,name in bundle.unresolved.items():
        if name in callbacks:c.hooks[at]=callbacks[name]
        elif name.startswith('?Wait@Semaphore@'):c.hooks[at]=semwait
        elif name.startswith('?Post@Semaphore@'):c.hooks[at]=sempost
    return c,trace


def compare(kind,index,both):
    if both[0]!=both[1]:
        errors.append({'kind':kind,'index':index,'return':[x[0] for x in both],
                       'trace':[x[2] for x in both],
                       'different_bytes':[[(i,a,b) for i,(a,b) in enumerate(zip(both[0][1],both[1][1])) if a!=b][:24]]})


mutex=Coff(paths['eathread_mutex.cpp'],'?Lock@Mutex@')
bundle=Bundle([mutex])
specs=[('?Init@Mutex@',0x82B42770,'mutex-init'),
       ('?Lock@Mutex@',0x82B427B0,'mutex-lock'),
       ('?Unlock@Mutex@',0x82B42838,'mutex-unlock'),
       ('??0Mutex@',0x82B43690,'mutex-ctor')]
for prefix,address,kind in specs:
    bundle.select(prefix)
    for index in range(128):
        image=bytearray(rng.randbytes(0x100));count=[0,1,2,16,-2][index%5]
        struct.pack_into('>i',image,0x20,count)
        now=[0,1,0xFFFFFF00,0xFFFFFFFF,0x12345678][index%5]
        deadline=[0,0xFFFFFFFF,(now+10000)&0xFFFFFFFF,(now+10001)&0xFFFFFFFF,
                  (now+20000)&0xFFFFFFFF][index%5]
        attempts=index%4
        both=[]
        for compiled in [False,True]:
            c,trace=fixture(bundle,now,attempts)
            c.failures=index%3;c.mem.wr(OBJ,image);wr32(c,AUX,deadline)
            c.setg(3,OBJ);c.setg(4,(AUX if kind=='mutex-lock' or index%2 else 0));c.setg(5,index%2)
            c.run(bundle.entry if compiled else address)
            both.append((c.g(3)&0xFFFFFFFF,c.mem.rd(OBJ,0x100),trace,c.msr))
        compare(kind,index,both)
    groups.append({'kind':kind,'cases':128})
    print(kind,'128 checked',flush=True)


condition=Coff(paths['eathread_condition.cpp'],'?Wait@Condition@')
bundle=Bundle([condition,mutex])
for prefix,address,kind in [('?Wait@Condition@',0x82B43388,'condition-wait'),
                            ('?Signal@Condition@',0x82B434D0,'condition-signal')]:
    bundle.select(prefix)
    for index in range(384):
        image=bytearray(rng.randbytes(0x100));external=bytearray(rng.randbytes(0x100))
        blocked=[0,1,3,100,0x3FFFFFFE,-1][index%6]
        unblock=[0,0,1,2,7][index%5]
        gone=[0,1,2,0x3FFFFFFE][index%4]
        struct.pack_into('>iii',image,0,blocked,unblock,gone)
        struct.pack_into('>i',image,0x50,0 if index%13 else -2)
        struct.pack_into('>i',external,0x20,[0,1,2,-1,7][index%5])
        result=[0,1,-1,-2,-3,0x3FFFFFFF][index%6]
        both=[]
        for compiled in [False,True]:
            c,trace=fixture(bundle,sem_result=result,relock_error=index%11==0)
            c.failures=index%3;c.mem.wr(OBJ,image);c.mem.wr(AUX+0x1000,external);wr32(c,AUX,0x12345678)
            c.setg(3,OBJ);c.setg(4,AUX+0x1000 if kind=='condition-wait' else index%2);c.setg(5,AUX)
            c.run(bundle.entry if compiled else address)
            both.append((c.g(3)&0xFFFFFFFF,c.mem.rd(OBJ,0x100)+c.mem.rd(AUX+0x1000,0x100),trace,c.msr))
        compare(kind,index,both)
    groups.append({'kind':kind,'cases':384})
    print(kind,'384 checked',flush=True)


rw=Coff(paths['eathread_rwmutex.cpp'],'?Lock@RWMutex@')
bundle=Bundle([rw,mutex])
for prefix,address,kind in [('?Lock@RWMutex@',0x82B43778,'rw-lock'),
                            ('?Unlock@RWMutex@',0x82B438A8,'rw-unlock')]:
    bundle.select(prefix)
    for index in range(256):
        image=bytearray(rng.randbytes(0x200))
        reads,writes,readers,writer=[index%4,index%3,index%6,0 if index%2 else 0x12345678]
        struct.pack_into('>iiii',image,0,reads,writes,readers,writer)
        struct.pack_into('>i',image,0x30,index%3)
        both=[]
        for compiled in [False,True]:
            c,trace=fixture(bundle);c.mem.wr(OBJ,image);wr32(c,AUX,0x12345678)
            waits=[0]
            def condwait(x):
                waits[0]+=1;assert waits[0]<=3
                trace.append(('condwait',x.g(3)&0xFFFFFFFF,x.g(4)&0xFFFFFFFF,x.rd32(x.g(5)&0xFFFFFFFF)))
                wr32(x,OBJ+8,0);wr32(x,OBJ+0xC,0)
                x.setg(3,[-2,-1,0,1][index%4])
            def condsignal(x):trace.append(('condsignal',x.g(3)&0xFFFFFFFF,x.g(4)&0xFFFFFFFF));x.setg(3,1)
            def threadid(x):trace.append(('threadid',));x.setg(3,0x13572468)
            c.hooks[0x82B43388]=condwait;c.hooks[0x82B434D0]=condsignal;c.hooks[0x82B42560]=threadid
            for at,name in bundle.unresolved.items():
                if name.startswith('?Wait@Condition@'):c.hooks[at]=condwait
                elif name.startswith('?Signal@Condition@'):c.hooks[at]=condsignal
                elif name.startswith('?GetThreadId@'):c.hooks[at]=threadid
            c.setg(3,OBJ);c.setg(4,index%4);c.setg(5,AUX)
            c.run(bundle.entry if compiled else address)
            both.append((c.g(3)&0xFFFFFFFF,c.mem.rd(OBJ,0x200),trace,c.msr))
        compare(kind,index,both)
    groups.append({'kind':kind,'cases':256})
    print(kind,'256 checked',flush=True)


result={'cases':sum(g['cases'] for g in groups),'groups':groups,'mismatches':errors,
        'objects':{str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in paths.values()},
        'scope':'complete compiled native functions vs original ARTIST instructions; full object bytes, returns, primitive arguments and MSR compared; failed reservations injected; recursive counts, deadline wrap, semaphore errors and condition/rw wait and signal branches',
        'limits':'controlled OS and semaphore/condition boundary callbacks; no hardware scheduling or concurrency; constructors of Condition/RWMutex and queue/thread startup are separate validation'}
(OUT/'synchronization-comparison.json').write_text(json.dumps(result,indent=2)+'\n')
print(result['cases'],'checks;',len(errors),'mismatches')
if errors:print(json.dumps(errors[:3],indent=2))
raise SystemExit(bool(errors))
