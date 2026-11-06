"""Native clock/sleep/thread-start instructions compared with ARTIST."""
import ast
import hashlib
import json
from pathlib import Path
import random
import struct
import sys

sys.path.insert(0,str(Path(__file__).parent))
import validate_native_math as M
from validate_render_states import Coff, CPU
from validate_resource_pointer import Bundle

ROOT=Path(__file__).resolve().parents[2]/'b5-decomp'
OUT=ROOT/'build/x360/native-thread-priority'
row=json.loads((OUT/'compile-results.json').read_text())['results'][0]
obj=Coff(Path(row['object']),'?GetThreadTime@')
bundle=Bundle([obj])
tree=ast.parse((Path(__file__).parent/'validate_native_strings.py').read_text())
nodes=[n for n in tree.body if isinstance(n,(ast.ClassDef,ast.FunctionDef)) and n.name in ['StringCPU','loaded_address']]
exec(compile(ast.Module(body=nodes,type_ignores=[]),'reservation-fixture','exec'),globals())
OBJ=M.S.SELF;AUX=M.S.AUX;NEW=AUX+0x1000;OLD=AUX+0x2000;PARAM=AUX+0x3000
rng=random.Random(360_1030_2)
errors=[];groups=[]
identity=json.loads((ROOT.parent/'progress/identity.json').read_text())

def put(c,a,v):c.mem.wr(a,struct.pack('>I',v&0xFFFFFFFF))
def fixture():
    c=bundle.cpu();c.__class__=StringCPU;c.msr=0x8000;c.reservation=None;c.failures=0
    c.mem.add(0x84800000,0x4000);c.mem.add(0x8324F000,0x2000);c.mem.add(0x82F86000,0x1000)
    return c

for prefix,address,kind in [('?GetThreadTime@',0x82B42628,'clock'),('?ThreadSleep@',0x82B42610,'sleep')]:
    bundle.select(prefix)
    for index in range(256):
        now=[0,1,0xFFFFFFFF,0xFFFFD8F0,0xFFFFFF00,rng.getrandbits(32)][index%6]
        duration=[0,0,1,1000,0xFFFFFFFF,rng.getrandbits(32)][index%6]
        value=rng.getrandbits(32)
        both=[]
        for compiled in [False,True]:
            c=fixture();trace=[];put(c,AUX,duration);c.setg(3,AUX)
            def tick(x):trace.append(('tick',));x.setg(3,now)
            def sleep(x):trace.append(('sleep-ex',x.g(3)&0xFFFFFFFF,x.g(4)&0xFFFFFFFF));x.setg(3,value)
            def switch(x):trace.append(('yield',));x.setg(3,value)
            callbacks={'GetTickCount':tick,'SleepEx':sleep,'SwitchToThread':switch}
            for name,fn in callbacks.items():
                for old in identity.get(name,{}).get('x360_addrs',[]):c.hooks[int(old,16)]=fn
            for at,name in bundle.unresolved.items():
                if name in callbacks:c.hooks[at]=callbacks[name]
            c.run(bundle.entry if compiled else address)
            both.append((c.g(3)&0xFFFFFFFF,c.mem.rd(AUX,4),trace,c.msr))
        if both[0]!=both[1]:errors.append({'kind':kind,'index':index,'outputs':repr(both)})
    groups.append({'kind':kind,'cases':256});print(kind,'256 checked',flush=True)

bundle.select('?Begin@Thread@')
roundrobin=loaded_address('glProcessorRoundRobin')
default=loaded_address('giDefaultProcessor')
entry=loaded_address('?ThreadEntryTrampoline@')
allocate=loaded_address('?AllocateThreadDynamicData@')
free=loaded_address('?FreeThreadDynamicData@')
for index in range(256):
    image=bytearray(rng.randbytes(0x4000))
    use_old=index%3!=0;use_params=index%4!=0
    old_ref=1 if index%2 else 3
    struct.pack_into('>I',image,0x2020,old_ref)
    name=0 if index%3==0 else PARAM+0x100
    struct.pack_into('>IIII',image,0x3000,0x12340000,[0,8192,0x10000][index%3],[-2,0,3][index%3]&0xFFFFFFFF,
                     [-1,0,1,5,6,7,99,0x7FFFFFFF][index%8]&0xFFFFFFFF)
    struct.pack_into('>I',image,0x3014,name)
    image[0x3100:0x3107]=b'worker\0'
    initial_round=[-1,0,1,5,6,7,0x7FFFFFFF][index%7]
    initial_default=[-1,0,3,6][index%4]
    handle=0 if index%7==0 else 0x87654320
    both=[]
    for compiled in [False,True]:
        c=fixture();c.failures=index%3;trace=[]
        c.mem.wr(AUX,image);put(c,OBJ,OLD if use_old else 0)
        put(c,roundrobin,initial_round);put(c,default,initial_default)
        put(c,0x8324FF0C,initial_round);put(c,0x82F862B0,initial_default)
        def alloc(x):trace.append(('allocate',));x.setg(3,NEW)
        def dealloc(x):trace.append(('free',x.g(3)&0xFFFFFFFF))
        def start(x):
            code=x.g(5)&0xFFFFFFFF
            assert code==(entry if compiled else 0x82B43EC8),(hex(code),hex(entry))
            trace.append(('begin',x.g(3)&0xFFFFFFFF,x.g(4)&0xFFFFFFFF,'entry',x.g(6)&0xFFFFFFFF,x.g(7)&0xFFFFFFFF,x.g(8)&0xFFFFFFFF))
            put(x,x.g(8)&0xFFFFFFFF,0x12345678);x.setg(3,handle)
        def priority(x):trace.append(('priority',x.g(3)&0xFFFFFFFF,x.g(4)&0xFFFFFFFF));x.setg(3,1)
        def processor(x):trace.append(('processor',x.g(3)&0xFFFFFFFF,x.g(4)&0xFFFFFFFF));x.setg(3,2)
        def resume(x):trace.append(('resume',x.g(3)&0xFFFFFFFF));x.setg(3,1)
        def copy(x):
            dest,source,size=[x.g(i)&0xFFFFFFFF for i in [3,4,5]]
            data=bytearray();ended=False
            for offset in range(size):
                ch=0 if ended else x.mem.rd(source+offset,1)[0]
                ended|=ch==0;data.append(ch)
            x.mem.wr(dest,data);x.setg(3,dest)
        def exception(x):
            args=x.g(6)&0xFFFFFFFF
            trace.append(('name',x.g(3)&0xFFFFFFFF,x.g(4)&0xFFFFFFFF,x.g(5)&0xFFFFFFFF,
                          tuple(x.rd32(args+i*4) for i in range(4))))
        c.hooks[0x82B43CF8]=alloc;c.hooks[allocate]=alloc
        c.hooks[0x82B43DD8]=dealloc;c.hooks[free]=dealloc
        callbacks={'_beginthreadex':start,'SetThreadPriority':priority,'XSetThreadProcessor':processor,
                   'ResumeThread':resume,'strncpy':copy,'RaiseException':exception}
        for name,fn in callbacks.items():
            for old in identity.get(name,{}).get('x360_addrs',[]):c.hooks[int(old,16)]=fn
        for at,name in bundle.unresolved.items():
            if name in callbacks:c.hooks[at]=callbacks[name]
        c.setg(3,OBJ);c.setg(4,0x40038000);c.setg(5,0x40039000);c.setg(6,PARAM if use_params else 0);c.setg(7,0 if index%2 else 0x4003A000)
        c.run(bundle.entry if compiled else 0x82B43F80)
        cursor=c.rd32(roundrobin if compiled else 0x8324FF0C)
        both.append((c.g(3)&0xFFFFFFFF,c.mem.rd(OBJ,4),c.mem.rd(AUX,0x4000),cursor,trace,c.msr))
    if both[0]!=both[1]:
        errors.append({'kind':'begin','index':index,'return':[o[0] for o in both],
                       'roundrobin':[o[3] for o in both],'trace':[o[4] for o in both],
                       'bytes':[(i,a,b) for i,(a,b) in enumerate(zip(both[0][2],both[1][2])) if a!=b][:12]})
groups.append({'kind':'thread-begin','cases':256});print('begin 256 checked',flush=True)

result={'cases':sum(g['cases'] for g in groups),'groups':groups,'mismatches':errors,
        'object':row['object'],'object_sha256':hashlib.sha256(Path(row['object']).read_bytes()).hexdigest(),
        'scope':'actual complete compiled functions vs ARTIST; return, whole thread records, atomic MSR, allocation/free and SDK call arguments; nullable params and names, failed begin, prior record lifetime, stack, priorities and processor cursor/default/wrap',
        'limits':'kernel allocation/thread/debugger services controlled; thread entry is validated as the correctly passed native code pointer, not run concurrently; allocation-null path not exercised'}
(OUT/'thread-comparison.json').write_text(json.dumps(result,indent=2)+'\n')
print(result['cases'],'checks;',len(errors),'mismatches')
if errors:print(json.dumps(errors[:4],indent=2))
raise SystemExit(bool(errors))
