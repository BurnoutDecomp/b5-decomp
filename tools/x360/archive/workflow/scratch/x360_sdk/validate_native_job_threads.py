"""Complete native worker lifecycle/virtual-dispatch comparisons against ARTIST."""
import hashlib,json,random,struct
from pathlib import Path
import native_atomic_fixture as F
ROOT=Path(__file__).resolve().parents[2]/'b5-decomp';OUT=ROOT/'build/x360/native-thread-join-final'
paths={Path(r['source']).name:Path(r['object']) for r in json.loads((ROOT/'build/x360/native-thread-join-complete/compile-results.json').read_text())['results'] if r['passed']}
bundle=F.Bundle([F.Coff(paths['job_thread.cpp'],'?Run@JobThread@'),F.Coff(paths['BrnEAThreadX360.cpp'],'??0Thread@')])
OBJ=F.M.S.SELF;AUX=F.M.S.AUX;BACKEND=AUX+0x1000;TABLE=AUX+0x2000
rng=random.Random(360_1030_11);errors=[];groups=[]
for prefix,original,kind in [('??0JobThread@',0x82BCAB38,'ctor'),('?Run@JobThread@',0x82BC9FB0,'run'),
                            ('?Start@JobThread@',0x82BC9EC8,'start'),('?WaitForEnd@JobThread@',0x82BC9F60,'wait')]:
    bundle.select(prefix)
    for index in range(128):
        image=bytearray(rng.randbytes(0x100));struct.pack_into('>I',image,4,BACKEND)
        image[8]=index%2;image[9]=int(index%7==0)
        struct.pack_into('>I',image,0x28,0x87654320)
        params=struct.pack('>IIIIII',index%2,index*4096,index,index%6,rng.getrandbits(32),AUX+0x7000)
        both=[]
        for compiled in [False,True]:
            c=F.cpu(bundle);c.mem.wr(OBJ,image);c.mem.wr(AUX,params);c.mem.wr(AUX+0x7000,b'worker\0')
            c.mem.wr(TABLE,b'\0'*0x60);F.put(c,BACKEND,TABLE)
            F.put(c,TABLE+0x50,AUX+0x3000);F.put(c,TABLE+0x54,AUX+0x3010)
            trace=[];state={'jobs':0}
            def create(x):
                trace.append(('event',*[x.g(r)&0xFFFFFFFF for r in range(3,7)]));x.setg(3,0x98765430)
            def jobparams(x):
                at=x.g(3)&0xFFFFFFFF
                x.mem.wr(at,struct.pack('>IIIIII',0,0,0,0xFFFFFFFF,63,AUX+0x7000));x.setg(3,at)
            def execute(x):
                state['jobs']+=1;trace.append(('execute',x.g(3)&0xFFFFFFFF,x.g(4)&0xFFFFFFFF))
                if state['jobs']>=1+index%3:x.mem.wr(OBJ+9,b'\x01')
                x.setg(3,[0,1,0x100,0xFFFFFFFF][index%4])
            def timeout(x):trace.append(('timeout',x.g(3)&0xFFFFFFFF));x.setg(3,[0,1,15,0xFFFFFFFF][index%4])
            def wait(x):trace.append(('event-wait',x.g(3)&0xFFFFFFFF,x.g(4)&0xFFFFFFFF));x.setg(3,0x102)
            def join(x):trace.append(('join',x.g(3)&0xFFFFFFFF,x.rd32(x.g(4)&0xFFFFFFFF),x.g(5)&0xFFFFFFFF));x.setg(3,2)
            def begin(x):
                p=x.g(6)&0xFFFFFFFF
                trace.append(('begin',x.g(3)&0xFFFFFFFF,x.g(4)&0xFFFFFFFF,x.g(5)&0xFFFFFFFF,
                              x.mem.rd(p,0x10).hex(),x.mem.rd(p+0x10,1).hex(),x.rd32(p+0x14),x.g(7)&0xFFFFFFFF))
                x.setg(3,0x12345678)
            def copy(x):
                dest,src,size=[x.g(r)&0xFFFFFFFF for r in [3,4,5]];x.mem.wr(dest,x.mem.rd(src,size));x.setg(3,dest)
            for a,fn in [(0x8292C808,wait),(0x82B42DD0,join),(0x82B442F8,begin),(0x828D6090,jobparams),(0x82C0A630,copy)]:c.hooks[a]=fn
            # Native CreateEventA import and class-wrapper word are attested by the ctor/start asm.
            imports=json.loads(Path('D:/Reverse/BP-Decomp_Workflow/.ida-exports/BURNOUT_X360_ARTIST.XEX/0x82BCAB38.json').read_text())['xrefs_from']
            for r in imports:
                if r['name']=='CreateEventA':c.hooks[int(r['address'],16)]=create
            c.hooks[AUX+0x3000]=execute;c.hooks[AUX+0x3010]=timeout
            c.mem.add(0x8324F000,0x2000);F.put(c,0x8324FF04,0x2468ACE0)
            if kind=='start':
                F.put(c,F.address(bundle,'?sGlobalRunnableClassUserWrapper@'),0x2468ACE0)
                c.hooks[F.address(bundle,'?Begin@Thread@1EA@@QAAPAXPAU')]=begin
            if kind=='wait':c.hooks[F.address(bundle,'?WaitForEnd@Thread@')]=join
            for at,name in bundle.unresolved.items():
                if name=='CreateEventA':c.hooks[at]=create
                elif name.startswith('??0JobThreadParameters@'):c.hooks[at]=jobparams
                elif name=='WaitForSingleObject':c.hooks[at]=wait
                elif name=='memcpy':c.hooks[at]=copy
            c.setg(3,OBJ);c.setg(4,AUX);c.setg(5,BACKEND)
            c.run(bundle.entry if compiled else original)
            result=bytearray(c.mem.rd(OBJ,0x100))
            if kind=='ctor':
                expected=F.address(bundle,'??_7JobThread@') if compiled else 0x821824D0
                assert c.rd32(OBJ)==expected,(hex(c.rd32(OBJ)),hex(expected));result[:4]=b'VPTR'
            both.append((c.g(3)&0xFFFFFFFF if kind in ['ctor','run'] else None,bytes(result),trace,c.msr))
        if both[0]!=both[1]:errors.append({'kind':kind,'index':index,'return':[o[0] for o in both],'trace':[o[2] for o in both],
                                         'bytes':[(i,a,b) for i,(a,b) in enumerate(zip(both[0][1],both[1][1])) if a!=b][:12]})
    groups.append({'kind':kind,'cases':128});print(kind,'128 checked',flush=True)
result={'cases':sum(g['cases'] for g in groups),'groups':groups,'mismatches':errors,
        'objects':{str(paths[name]):hashlib.sha256(paths[name].read_bytes()).hexdigest() for name in ['job_thread.cpp','BrnEAThreadX360.cpp']},
        'scope':'whole actual JobThread ctor/run/start/wait vs ARTIST; exact2C stride,flags,event setup,IRunnable Begin ABI,24byte parameter copy,timeout/join arguments; real virtual slots50/54 tested with all other slots null and boolean low-byte return cases',
        'limits':'event/thread/parameter-constructor/copy services controlled;constructor vptr normalized only after expected-address assertion;no physical worker scheduling'}
(OUT/'job-thread-comparison.json').write_text(json.dumps(result,indent=2)+'\n')
print(result['cases'],'checks;',len(errors),'mismatches')
if errors:print(json.dumps(errors[:3],indent=2))
raise SystemExit(bool(errors))
