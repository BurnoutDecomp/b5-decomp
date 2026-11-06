"""Execute native queue response waits with exact ARTIST field/ABI witnesses."""
import ast
import hashlib
import json
from pathlib import Path
import random
import struct
import sys

sys.path.insert(0,str(Path(__file__).parent))
import validate_native_math as M
from validate_render_states import Coff,CPU
from validate_resource_pointer import Bundle
ROOT=Path(__file__).resolve().parents[2]/'b5-decomp'
OUT=ROOT/'build/x360/native-thread-sync-complete'
rows=json.loads((OUT/'compile-results.json').read_text())['results']
paths={Path(r['source']).name:Path(r['object']) for r in rows if r['passed']}
bundle=Bundle([Coff(paths['RealmcMessageQueue.cpp'],'?_WaitForResponse@'),
               Coff(paths['RealmcCore.cpp'],'?EMPTY_RESPONSE@ResponsePtr@')])
tree=ast.parse((Path(__file__).parent/'validate_native_strings.py').read_text())
nodes=[n for n in tree.body if isinstance(n,(ast.ClassDef,ast.FunctionDef)) and n.name in ['StringCPU','loaded_address']]
exec(compile(ast.Module(body=nodes,type_ignores=[]),'reservation-fixture','exec'),globals())
bundle.select('?_WaitForResponse@')
global_empty=loaded_address('?g_pRealmcEmptyResponse@')
vtable=loaded_address('??_7ResponsePtr@')
extract=loaded_address('?_ExtractResponse@')
OBJ=M.S.SELF;AUX=M.S.AUX;HOLDER=AUX+0x1000;EMPTY=AUX+0x1100;MSG=AUX+0x1200;NODE=AUX+0x1300
rng=random.Random(360_1030_3)
errors=[]

def put(c,a,v):c.mem.wr(a,struct.pack('>I',v&0xFFFFFFFF))
for index in range(256):
    image=bytearray(rng.randbytes(0x200))
    struct.pack_into('>II',image,0xC,(NODE if index%3 else OBJ+0xC),OBJ+0xC)
    image[0x18]=1;image[0x19]=int(index%11==0)
    struct.pack_into('>I',image,0xB0,[0,1,6000,0x7FFFFFFF,0xFFFFFFFF][index%5])
    ticks=[0,0xFFFFFFFF,0xFFFFFF00,rng.getrandbits(32)][index%4]
    both=[]
    for compiled in [False,True]:
        c=bundle.cpu();c.__class__=StringCPU;c.msr=0x8000;c.reservation=None;c.failures=index%3
        c.mem.add(0x84800000,0x4000);c.mem.add(0x832BE000,0x1000)
        c.mem.wr(OBJ,image);c.mem.wr(AUX,b'\xCC'*0x2000)
        put(c,HOLDER+4,EMPTY);put(c,EMPTY+4,123);put(c,MSG+4,EMPTY)
        put(c,0x832BE1F4,HOLDER);put(c,global_empty,HOLDER)
        trace=[];state={'waits':0,'extracts':0}
        def lock(x):
            trace.append(('lock',x.g(3)&0xFFFFFFFF,x.rd32(x.g(4)&0xFFFFFFFF)));x.setg(3,1)
        def unlock(x):trace.append(('unlock',x.g(3)&0xFFFFFFFF));x.setg(3,0)
        def signal(x):trace.append(('signal',x.g(3)&0xFFFFFFFF,x.g(4)&0xFFFFFFFF));x.setg(3,1)
        def gettime(x):trace.append(('time',));x.setg(3,(ticks+10000)&0xFFFFFFFF)
        def take(x):
            state['extracts']+=1
            trace.append(('extract',x.g(3)&0xFFFFFFFF,x.g(4)&0xFFFFFFFF,x.g(5)&0xFFFFFFFF))
            found=state['waits']>=index%3
            if found and index%2:put(x,OBJ+0xC,OBJ+0xC)
            x.setg(3,int(found))
        def wait(x):
            state['waits']+=1;assert state['waits']<=4,(index,trace)
            trace.append(('wait',x.g(3)&0xFFFFFFFF,x.g(4)&0xFFFFFFFF,x.rd32(x.g(5)&0xFFFFFFFF)))
            put(x,OBJ+0xC,NODE)
            value=-1 if index%7==0 else [-2,0,1][index%3]
            x.setg(3,value)
        old={0x82B427B0:lock,0x82B42838:unlock,0x82B434D0:signal,0x82B42628:gettime,
             0x82B43388:wait,0x82C46928:take}
        # The actual extractor's entry is an export; derive it by identity.
        identity=json.loads((ROOT.parent/'progress/identity.json').read_text())
        for a in identity['RealmcCore::MessageQueue::_ExtractResponse']['x360_addrs']:old[int(a,16)]=take
        for a,fn in old.items():c.hooks[a]=fn
        c.hooks[extract]=take
        for at,name in bundle.unresolved.items():
            if name.startswith('?Lock@Mutex@'):c.hooks[at]=lock
            elif name.startswith('?Unlock@Mutex@'):c.hooks[at]=unlock
            elif name.startswith('?Signal@Condition@'):c.hooks[at]=signal
            elif name.startswith('?GetThreadTime@'):c.hooks[at]=gettime
            elif name.startswith('?Wait@Condition@'):c.hooks[at]=wait
        c.setg(3,AUX);c.setg(4,OBJ);c.setg(5,MSG)
        c.run(bundle.entry if compiled else 0x82C471F0)
        result=bytearray(c.mem.rd(AUX,8))
        actual_vtable=struct.unpack_from('>I',result)[0]
        assert actual_vtable==(vtable if compiled else 0x821BA308),(hex(actual_vtable),hex(vtable))
        result[:4]=b'VPTR'
        both.append((c.g(3)&0xFFFFFFFF,c.mem.rd(OBJ,0x200),bytes(result),c.rd32(EMPTY+4),trace,c.msr))
    if both[0]!=both[1]:errors.append({'index':index,'return':[o[0] for o in both],'trace':[o[4] for o in both],
                                     'bytes':[(i,a,b) for i,(a,b) in enumerate(zip(both[0][1],both[1][1])) if a!=b][:12]})

report={'cases':256,'mismatches':errors,'objects':{str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in paths.values()},
        'scope':'actual whole queue wait function and real compiled ResponsePtr copy/refcount initialization vs ARTIST; exact hidden-return ABI, field offsets, biased/wrapping u32 deadlines, repeated timeout retry, -1 shutdown, extraction and broadcast; full queue, result, empty response references, returns, calls and MSR',
        'limits':'mutex/condition/extraction services controlled; result vtable addresses normalized only after explicit expected-address checks; no physical threads, console launch or full allocator/response-list implementation proof'}
(OUT/'queue-comparison.json').write_text(json.dumps(report,indent=2)+'\n')
print('queue 256 checks;',len(errors),'mismatches')
if errors:print(json.dumps(errors[:3],indent=2))
raise SystemExit(bool(errors))
