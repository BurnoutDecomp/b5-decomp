"""Execute both original/compiled entry paths and allocation-failure behavior."""
import hashlib,json,random,struct
from pathlib import Path
import native_atomic_fixture as F
ROOT=Path(__file__).resolve().parents[2]/'b5-decomp';OUT=ROOT/'build/x360/native-thread-join-final'
path=Path(next(r['object'] for r in json.loads((ROOT/'build/x360/native-thread-join-complete/compile-results.json').read_text())['results'] if r['source'].endswith('BrnEAThreadX360.cpp')))
bundle=F.Bundle([F.Coff(path,'?ThreadClassEntryTrampoline@')]);OBJ=F.M.S.SELF;AUX=F.M.S.AUX
rng=random.Random(360_1030_10);errors=[];groups=[]
for class_path in [False,True]:
    bundle.select('?ThreadClassEntryTrampoline@' if class_path else '?ThreadEntryTrampoline@')
    tls=F.address(bundle,'gdwThreadHandleTls');table=F.address(bundle,'gThreadHandleTable');free=F.address(bundle,'?FreeThreadDynamicData@')
    for index in range(128):
        image=bytearray(rng.randbytes(0x100));data=bytearray(rng.randbytes(0x1000))
        runnable=AUX+0x100;context=AUX+0x300;wrapper=0 if index%2 else AUX+0x400
        struct.pack_into('>II',data,0x100,AUX+0x180,0);struct.pack_into('>I',data,0x184,AUX+0x200)
        struct.pack_into('>IIIII',image,0x10,runnable,context,0x87654320,wrapper,[0,1,2,5][index%4])
        result=rng.getrandbits(32);both=[]
        for compiled in [False,True]:
            c=F.cpu(bundle);c.failures=index%3;c.mem.wr(OBJ,image);c.mem.wr(AUX,data);trace=[]
            c.mem.add(0x82F86000,0x1000);F.put(c,tls,7);F.put(c,0x82F862A8,7)
            def sethandle(x):
                trace.append(('current',x.g(3)&0xFFFFFFFF,x.g(4)&0xFFFFFFFF,x.rd32(OBJ+8),x.rd32(OBJ+0xC)))
                # Entry arguments must already be captured before this boundary.
                if x.g(4)&0xFFFFFFFF:
                    F.put(x,OBJ+0x10,AUX+0x700);F.put(x,OBJ+0x14,AUX+0x800)
                elif index%7==0:F.put(x,OBJ+0xC,0xCAFEBABE)
            def run(x):
                trace.append(('run',x.g(3)&0xFFFFFFFF,x.g(4)&0xFFFFFFFF if class_path else None,x.rd32(OBJ+8)))
                x.setg(3,result)
            def wrapped(x):
                trace.append(('wrapper',x.g(3)&0xFFFFFFFF,x.g(4)&0xFFFFFFFF,x.rd32(OBJ+8)))
                x.setg(3,result)
            def dealloc(x):trace.append(('free',x.g(3)&0xFFFFFFFF,x.rd32(OBJ+8)));x.setg(3,0x123)
            def checktable(x):
                assert x.g(3)&0xFFFFFFFF==(table if compiled else 0x8324FF10)
                trace.append(('table-check',x.g(4)&0xFFFFFFFF))
            c.hooks[0x82B42328]=checktable
            c.hooks[0x82C76B0C]=sethandle
            for at,name in bundle.unresolved.items():
                if name=='TlsSetValue':c.hooks[at]=sethandle
                elif name.startswith('?CheckDynamicThreadArray@'):c.hooks[at]=checktable
            for at in [free,0x82B43DD8]:c.hooks[at]=dealloc
            c.hooks[AUX+0x200 if class_path else runnable]=run;c.hooks[AUX+0x400]=wrapped
            c.setg(3,OBJ);c.run(bundle.entry if compiled else (0x82B44248 if class_path else 0x82B43EC8))
            both.append((c.g(3)&0xFFFFFFFF,c.mem.rd(OBJ,0x100),trace,c.msr,c.atomic_trace,c.fence_trace))
        if both[0]!=both[1]:errors.append({'kind':'class-entry' if class_path else 'function-entry','index':index,
                                         'return':[o[0] for o in both],'trace':[o[2] for o in both],'atomic':[o[4] for o in both]})
    groups.append({'kind':'class-entry' if class_path else 'function-entry','cases':128})
    print(groups[-1]['kind'],'128 checked',flush=True)

bundle.select('?GetGlobalRunnableClassUserWrapper@')
variable=F.address(bundle,'?sGlobalRunnableClassUserWrapper@')
for index in range(64):
    value=[0,1,0xFFFFFFFF,0x87654320,rng.getrandbits(32)][index%5];both=[]
    for compiled in [False,True]:
        c=F.cpu(bundle);c.mem.add(0x8324F000,0x2000);F.put(c,variable,value);F.put(c,0x8324FF04,value)
        c.run(bundle.entry if compiled else 0x82B42DB0);both.append(c.g(3)&0xFFFFFFFF)
    if both[0]!=both[1]:errors.append({'kind':'class-global','index':index,'outputs':both})
groups.append({'kind':'class-global','cases':64})

class Fault(Exception):pass
for class_path in [False,True]:
    bundle.select('?Begin@Thread@1EA@@QAAPAXPAU' if class_path else '?Begin@Thread@1EA@@QAAPAXP6A')
    allocate=F.address(bundle,'?AllocateThreadDynamicData@')
    for index in range(16):
        both=[]
        for compiled in [False,True]:
            c=F.cpu(bundle);F.put(c,OBJ,0);trace=[];original=c.mem.rd
            def read(at,size):
                if at<0x1000:trace.append(('fault',at,size));raise Fault()
                return original(at,size)
            c.mem.rd=read
            def alloc(x):trace.append(('alloc',));x.setg(3,0)
            c.hooks[allocate]=alloc;c.hooks[0x82B43CF8]=alloc
            c.setg(3,OBJ);c.setg(4,AUX+0x100);c.setg(5,0);c.setg(6,0);c.setg(7,0)
            fault=False
            try:c.run(bundle.entry if compiled else (0x82B442F8 if class_path else 0x82B43F80))
            except Fault:fault=True
            c.mem.rd=original
            both.append((fault,c.rd32(OBJ),trace,c.msr))
        if both[0]!=both[1]:errors.append({'kind':'allocation-null','class':class_path,'index':index,'outputs':both})
    groups.append({'kind':'class-allocation-null' if class_path else 'function-allocation-null','cases':16})

report={'cases':sum(g['cases'] for g in groups),'groups':groups,'mismatches':errors,
        'object':str(path),'object_sha256':hashlib.sha256(path.read_bytes()).hexdigest(),
        'scope':'complete actual native function/class entry paths and getter vs ARTIST; arguments captured before handle publication,status/return/free/refcount/MSR/fences,wrapper vs virtual slot4,mutable class global,real null-allocation fault and owner pointer state',
        'limits':'thread callbacks/handle and allocation services controlled; low memory is deliberately unmapped for failure witness; no physical threads'}
(OUT/'entry-comparison.json').write_text(json.dumps(report,indent=2)+'\n')
print(report['cases'],'checks;',len(errors),'mismatches')
if errors:print(json.dumps(errors[:3],indent=2))
raise SystemExit(bool(errors))
