import os
"""Complete native barrier executions, including the timeout and error arms."""
import hashlib,json,random,struct
from pathlib import Path
import native_atomic_fixture as F
ROOT=Path(__file__).resolve().parents[2]/'b5-decomp'
OUT=ROOT/'build/x360/native-frame-final'
REPORT=Path(os.environ.get('NATIVE_FRAME_REPORT',str(OUT/'compile-results.json')))
paths={Path(r['source']).name:Path(r['object']) for r in json.loads(REPORT.read_text())['results'] if r['passed']}
if 'BrnEAThreadX360.cpp' not in paths:
    prior=json.loads((OUT/'compile-results.json').read_text())['results']
    paths['BrnEAThreadX360.cpp']=Path(next(r['object'] for r in prior if r['source'].endswith('BrnEAThreadX360.cpp')))
bundle=F.Bundle([F.Coff(paths['eathread_barrier.cpp'],'?Wait@Barrier@'),
                 F.Coff(paths['eathread_barrier_data.cpp'],'??0EABarrierData@@'),
                 F.Coff(paths['BrnEAThreadX360.cpp'],'??0BarrierParameters@')])
OBJ=F.M.S.SELF;AUX=F.M.S.AUX
rng=random.Random(360_1030_5)
errors=[];groups=[]
identity=json.loads((ROOT.parent/'progress/identity.json').read_text())
for prefix,original,kind,count in [('?Wait@Barrier@',0x82B43168,'wait',512),
                                  ('?Init@Barrier@',0x82B430C8,'init',128),
                                  ('??0EABarrierData@@',0x82B43A50,'data-ctor',64),
                                  ('??0Barrier@',0x82B43AD8,'ctor',64)]:
    bundle.select(prefix)
    for index in range(count):
        image=bytearray(rng.randbytes(0x100))
        current=[0,1,2,3][index%4];height=[0,1,2,16][(index//4)%4];phase=(index//16)%2
        struct.pack_into('>iii',image,0,current,height,phase)
        deadline=[0,0xFFFFFFFF,10000,0xFFFFFF00][index%4]
        sem_result=[0,1,10,-1,-2,-3,0x7FFFFFFF][(index//2)%7]
        both=[]
        for compiled in [False,True]:
            c=F.cpu(bundle);c.failures=index%3;c.mem.wr(OBJ,image);F.put(c,AUX,deadline)
            # Incoming BarrierParameters: height, intra-process, first name byte.
            c.mem.wr(AUX+0x100,struct.pack('>iBB',index%7,1,0))
            trace=[]
            def wait(x):
                trace.append(('wait',x.g(3)&0xFFFFFFFF,x.rd32(x.g(4)&0xFFFFFFFF)))
                if index%5==0:F.put(x,OBJ+8,1-phase)
                x.setg(3,sem_result)
            def post(x):
                trace.append(('post',x.g(3)&0xFFFFFFFF,x.g(4)&0xFFFFFFFF))
                if index%5==0:F.put(x,OBJ+8,1-phase)
                x.setg(3,sem_result)
            def seminit(x):
                target=x.g(3)&0xFFFFFFFF;params=x.g(4)&0xFFFFFFFF
                trace.append(('init',target,x.mem.rd(params,5).hex(),F.cstring(x,params+5)))
                x.setg(3,index%2)
            def semctor(x):
                target=x.g(3)&0xFFFFFFFF
                trace.append(('semctor',target,x.g(4)&0xFFFFFFFF,x.g(5)&0xFFFFFFFF))
                for offset in [0,4,8]:F.put(x,target+offset,0)
                x.mem.wr(target+0xC,b'\x01');x.setg(3,target)
            c.hooks[0x82B42A40]=wait;c.hooks[0x82B42B70]=post;c.hooks[0x82B42988]=seminit;c.hooks[0x82B43960]=semctor
            for at,name in bundle.unresolved.items():
                if name.startswith('?Wait@Semaphore@'):c.hooks[at]=wait
                elif name.startswith('?Post@Semaphore@'):c.hooks[at]=post
                elif name.startswith('?Init@Semaphore@'):c.hooks[at]=seminit
                elif name.startswith('??0Semaphore@'):c.hooks[at]=semctor
            c.setg(3,OBJ);c.setg(4,AUX if kind=='wait' else (AUX+0x100 if index%2 else 0));c.setg(5,index%2)
            c.run(bundle.entry if compiled else original)
            both.append((c.g(3)&0xFFFFFFFF,c.mem.rd(OBJ,0x100),trace,c.msr,c.atomic_trace))
        if both[0]!=both[1]:errors.append({'kind':kind,'index':index,'return':[o[0] for o in both],
                                         'trace':[o[2] for o in both],'atomic':[o[4] for o in both],
                                         'bytes':[(i,a,b) for i,(a,b) in enumerate(zip(both[0][1],both[1][1])) if a!=b][:12]})
    print(kind,count,'checked',flush=True);groups.append({'kind':kind,'cases':count})
result={'cases':sum(g['cases'] for g in groups),'groups':groups,'mismatches':errors,
        'objects':{str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in paths.values()},
        'scope':'complete native barrier init/wait/data ctor/ctor against ARTIST; real biased timeout passed unchanged to semaphore; failure codes,phase changes and failed reservations; full object,return,callbacks,MSR and every store-conditional attempt',
        'limits':'semaphore operations controlled; no hardware concurrency; phase changes injected at semaphore boundary'}
(OUT/'barrier-comparison.json').write_text(json.dumps(result,indent=2)+'\n')
print(result['cases'],'checks;',len(errors),'mismatches')
if errors:print(json.dumps(errors[:3],indent=2))
raise SystemExit(bool(errors))
