"""Actual XDK objects versus ARTIST: scheduler slots, return ABI, argument overlay."""
import hashlib,json,random,struct
from pathlib import Path
import native_atomic_fixture as F

ROOT=Path(__file__).resolve().parents[2]/'b5-decomp'
OUT=ROOT/'build/x360/native-thread-join-final'
paths={Path(r['source']).name:Path(r['object']) for r in json.loads((ROOT/'build/x360/native-thread-join-complete/compile-results.json').read_text())['results'] if r['passed']}
objects=[F.Coff(paths['local_backend.cpp'],'?Run@JobInstance@'),F.Coff(paths['job_instance_handle.cpp'],'?ExecuteReadyJobs@SchedulerBackend@')]
bundle=F.Bundle(objects)
OBJ=F.M.S.SELF;AUX=F.M.S.AUX
rng=random.Random(360_1030_12);errors=[];groups=[];slots=[]
raw=F.M.V.x360rd
names=['~','GetNumJobsInFlight','GetNumThreads','GetThreadHandle','AddThread','RequestEnd','WaitForEnd','WakeThreads',
       'GetThreadId','GetThreadParameters','CreateNotReadyInstance','GetEnabler','IsJobComplete','SubmitEvent','AddBarrier',
       'SleepOn','GetAllowSleepOn','SetProfilingCallback','SetProfilingContext','FlushProfile','ExecuteReadyJobs','GetSleepTimeoutMs']
tables=json.loads((Path(__file__).parent/'NATIVE_SCHEDULER_VTABLES.json').read_text())
for oi,table,owner in [(0,0x82182500,'LocalBackend@0Jobs@EA@@'),(1,0x82182408,'SchedulerBackend@Detail@Jobs@EA@@')]:
    obj=objects[oi];matches=[(n,v,s) for n,v,s in obj.symbols.values() if n=='??_7'+owner+'6B@']
    assert len(matches)==1,matches
    _,value,section=matches[0];relocs=sorted(obj.relocations(section));assert len(relocs)==22
    for i,(at,symbol,kind) in enumerate(relocs):
        name,_,_=obj.symbols[symbol];expected=names[i]
        assert at==value+i*4 and kind==2,(at,kind)
        entry=tables[hex(table)][i];assert raw.u32(table+i*4)==int(entry['address'],16)
        if i==0:assert name.startswith('??_E'+owner)
        elif oi==1 and i not in [7,17,18,19,20]:assert name=='_purecall',(i,name)
        else:assert name.startswith('?'+expected+'@'),(i,name,expected)
        slots.append({'table':hex(table),'slot':i*4,'compiled_symbol':name,'original':entry['address']})
    if oi==1:
        for i in [7,17,18,19,20]:
            prefix='?'+names[i]+'@SchedulerBackend@';bundle.select(prefix)
            expected=[0x38600000,0x4e800020] if i==20 else [0x4e800020]
            assert [raw.u32(int(tables[hex(table)][i]['address'],16)+4*j) for j in range(len(expected))]==expected
            at=bundle.entry
            assert [struct.unpack_from('>I',bundle.images[at],j*4)[0] for j in range(len(expected))]==expected

def copy(x):
    dest,src,size=[x.g(r)&0xffffffff for r in [3,4,5]]
    x.mem.wr(dest,x.mem.rd(src,size));x.setg(3,dest)

def mismatch(kind,index,both):
    if both[0]!=both[1]:
        errors.append({'kind':kind,'index':index,'return':[b[0] for b in both],'trace':[b[2] for b in both],
                       'bytes':[(i,a,b) for i,(a,b) in enumerate(zip(both[0][1],both[1][1])) if a!=b][:16]})

# Whole Run, with service boundaries controlled; all four entry arguments must be raw r3-r6 words.
bundle.select('?Run@JobInstance@')
play=F.address(bundle,'?PlayEventList@JobInstance@')
for index in range(128):
    image=bytearray(rng.randbytes(0x290));image[0x28]=int(index%3==0)
    struct.pack_into('>I',image,0x2c,0 if index%5==0 else AUX+0x1000)
    before=rng.getrandbits(64);after=rng.getrandbits(64);handle=rng.getrandbits(32);both=[]
    for compiled in [False,True]:
        c=F.cpu(bundle);c.mem.wr(OBJ,image);trace=[];state={'queries':0}
        def event(x):
            when=x.g(4)&0xffffffff;trace.append(('event',x.g(3)&0xffffffff,when,x.mem.rd(OBJ+0x38,24).hex()))
            x.mem.wr(OBJ+0x288+when,b'\x01')
        def query(x):
            state['queries']+=1
            trace.append(('query',state['queries'],x.mem.rd(OBJ+0x38,24).hex()))
            F.put(x,x.g(3)&0xffffffff,before if state['queries']==1 else after,8);x.setg(3,1)
        def job(x):
            trace.append(('entry',*[x.g(r)&0xffffffff for r in range(3,7)]));F.put(x,OBJ+0x28,0)
        def threadid(x):trace.append(('thread-id',));x.setg(3,0x1234567800000000|handle)
        def debug(x):trace.append(('break',))
        for at,fn in [(0x82BCBC00,event),(play,event),(0x82926EC8,query),(0x82B42560,threadid),(0x82C208E8,debug),(AUX+0x1000,job)]:c.hooks[at]=fn
        for at,name in bundle.unresolved.items():
            if name=='QueryPerformanceCounter':c.hooks[at]=query
            elif name=='DebugBreak':c.hooks[at]=debug
            elif name.startswith('?GetThreadId@Thread@EA@@'):c.hooks[at]=threadid
        c.setg(3,OBJ);c.run(bundle.entry if compiled else 0x82BCC2F0)
        both.append((None,c.mem.rd(OBJ,0x290),trace))
    mismatch('instance-run',index,both)
groups.append({'kind':'instance-run','cases':128});print('instance-run128 checked',flush=True)

# False semaphore arm with empty overflow chains; actual native initialization copies/profile overlay.
bundle.select('?Initialize@JobInstance@')
for index in range(128):
    image=bytearray(rng.randbytes(0x290));params=bytearray(rng.randbytes(44));params[0x25]=0
    struct.pack_into('>I',image,0x150,0);struct.pack_into('>I',image,0x260,0)
    args=rng.randbytes(16);counter=rng.getrandbits(64);both=[]
    for compiled in [False,True]:
        c=F.cpu(bundle);c.mem.wr(OBJ,image);c.mem.wr(AUX,params);c.mem.wr(AUX+0x100,args);trace=[]
        def query(x):
            trace.append(('query',x.mem.rd(OBJ+4,44).hex()));F.put(x,x.g(3)&0xffffffff,counter,8);x.setg(3,1)
        def clear(x):
            at=x.g(3)&0xffffffff;assert at in [OBJ+0x50,OBJ+0x160]
            F.put(x,at+0x100,0);F.put(x,at+0x104,0)
        for at,name in bundle.unresolved.items():
            if name=='memcpy':c.hooks[at]=copy
            elif name=='QueryPerformanceCounter':c.hooks[at]=query
            elif name.startswith('?Clear@') and 'BucketListNode' in name:c.hooks[at]=clear
        c.hooks[0x82C0A630]=copy;c.hooks[0x82926EC8]=query
        c.setg(3,OBJ);c.setg(4,AUX);c.setg(5,AUX+0x100);c.run(bundle.entry if compiled else 0x82BCAA18)
        both.append((None,c.mem.rd(OBJ,0x290),trace))
    mismatch('instance-initialize',index,both)
groups.append({'kind':'instance-initialize-no-semaphore','cases':128});print('instance-initialize128 checked',flush=True)

# Whole backend accessors and worker forwarding. Native handle is a u64 with low32 index.
specs=[('GetNumThreads',0x82BC9D58,'count'),('GetThreadHandle',0x82BC9E20,'handle'),
       ('AddThread',0x82BCA6C0,'add'),('RequestEnd',0x82BCBA40,'request'),
       ('WaitForEnd',0x82BCA738,'wait'),('GetThreadId',0x82BC9E30,'id'),
       ('GetThreadParameters',0x82BCA770,'params')]
exports=Path('D:/Reverse/BP-Decomp_Workflow/.ida-exports/BURNOUT_X360_ARTIST.XEX')
id_export=json.loads((exports/'0x82BC9E30.json').read_text())
id_addr=next(int(x['address'],16) for x in id_export['xrefs_from'] if 'GetId' in str(x['name']))
for name,original,kind in specs:
    bundle.select('?'+name+'@LocalBackend@1Jobs@EA@@')
    for index in range(128):
        image=bytearray(rng.randbytes(0x600));worker=index%32;handle=(rng.getrandbits(32)<<32)|worker
        idle=index%33
        for i in range(32):image[0x1c+i*0x2c]=int(i<idle)
        params=rng.randbytes(24);result=rng.getrandbits(32);both=[]
        for compiled in [False,True]:
            c=F.cpu(bundle);c.mem.wr(OBJ,image);c.mem.wr(AUX+0x100,params);c.mem.wr(AUX,b'\xAA'*32);trace=[]
            def start(x):
                trace.append(('start',x.g(3)&0xffffffff,x.g(4)&0xffffffff,x.g(5)&0xffffffff,
                              x.mem.rd(x.g(4)&0xffffffff,24).hex()));x.mem.wr((x.g(3)&0xffffffff)+8,b'\x01')
            def wait(x):trace.append(('wait',x.g(3)&0xffffffff))
            def getid(x):trace.append(('getid',x.g(3)&0xffffffff));x.setg(3,0xfedcba9800000000|result)
            def event(x):trace.append(('setevent',x.g(3)&0xffffffff,c.mem.rd(OBJ,0x600).hex()))
            for at,fn in [(0x82BC9EC8,start),(0x82BC9F60,wait),(id_addr,getid),(0x82C0A630,copy)]:c.hooks[at]=fn
            # Resolve the actual import address from raw RequestEnd's branch.
            word=raw.u32(0x82BCBA68);delta=word&0x03fffffc
            if delta&0x02000000:delta-=0x04000000
            assert word>>26==18 and (0x82BCBA68+delta)&0xffffffff==0x82C205A8
            c.hooks[0x82C205A8]=event
            for at,symbol in bundle.unresolved.items():
                if symbol.startswith('?Start@JobThread@'):c.hooks[at]=start
                elif symbol.startswith('?WaitForEnd@JobThread@'):c.hooks[at]=wait
                elif symbol.startswith('?GetId@Thread@'):c.hooks[at]=getid
                elif symbol=='memcpy':c.hooks[at]=copy
                elif symbol=='SetEvent':c.hooks[at]=event
            if kind in ['handle','add','params']:
                c.setg(3,AUX);c.setg(4,OBJ);c.setg(5,AUX+0x100 if kind=='add' else (worker if kind=='handle' else handle))
            else:c.setg(3,OBJ);c.setg(4,handle)
            c.run(bundle.entry if compiled else original)
            returned=c.g(3)&(0xffffffffffffffff if kind=='id' else 0xffffffff) if kind in ['count','handle','add','id','params'] else None
            both.append((returned,c.mem.rd(OBJ,0x600)+c.mem.rd(AUX,32),trace))
        mismatch(kind,index,both)
    groups.append({'kind':kind,'cases':128});print(kind,'128 checked',flush=True)

# Whole slot acquisition, including the native16-byte hidden return buffer and watchdog abort.
bundle.select('?CreateNotReadyInstance@LocalBackend@')
initialize=F.address(bundle,'?Initialize@JobInstance@')
collect=F.address(bundle,'?TryToRunJobInstanceGarbageCollector@')
watchdog=F.address(bundle,'spGlobalAbortPredicate')
for index in range(128):
    image=bytearray(rng.randbytes(0x600));count=1+index%7;target=index%count;abort=index%4==0
    struct.pack_into('>IIII',image,4,AUX+0x2000,AUX+0x1000,(target-1)&0xffffffff,count)
    struct.pack_into('>Q',image,0x598,rng.getrandbits(64))
    params=bytearray(rng.randbytes(44));struct.pack_into('>II',params,0x10,rng.getrandbits(32),rng.getrandbits(32))
    args=rng.randbytes(16);queue=bytearray(b'\0'*(count*8))
    if not abort:struct.pack_into('>Q',queue,target*8,0xffffffffffffffff)
    both=[]
    for compiled in [False,True]:
        c=F.cpu(bundle);c.failures=index%3;c.mem.wr(OBJ,image);c.mem.wr(AUX,b'\xAA'*32)
        c.mem.wr(AUX+0x100,params);c.mem.wr(AUX+0x200,args);c.mem.wr(AUX+0x1000,queue)
        c.mem.wr(AUX+0x2000,b'\x55'*(count*0x290));c.mem.add(0x8327F200,0x200)
        F.put(c,watchdog,AUX+0x900);F.put(c,0x8327F284,AUX+0x900)
        trace=[];state={'queries':0}
        def initialize_job(x):
            dest,p,a=[x.g(r)&0xffffffff for r in [3,4,5]]
            trace.append(('initialize',dest,p,a));x.mem.wr(dest+4,x.mem.rd(p,44));x.mem.wr(dest+0x38,x.mem.rd(a,16))
        def gc(x):trace.append(('gc',x.g(3)&0xffffffff,x.g(4)&0xffffffff))
        def query(x):
            state['queries']+=1;trace.append(('query',state['queries']));F.put(x,x.g(3)&0xffffffff,100*state['queries'],8);x.setg(3,1)
        def seconds(x):trace.append(('seconds',x.g(3)));x.setf(1,20.0)
        def abort_job(x):trace.append(('abort',));x.setg(3,0)
        def priority(x):x.setg(3,x.rd32((x.g(3)&0xffffffff)+0x10))
        def affinity(x):x.setg(3,x.rd32((x.g(3)&0xffffffff)+0x14))
        for at,fn in [(initialize,initialize_job),(0x82BCAA18,initialize_job),(collect,gc),(0x82BCB718,gc),
                      (0x82926EC8,query),(AUX+0x900,abort_job)]:c.hooks[at]=fn
        original_export=json.loads((exports/'0x82BCC010.json').read_text())
        for row in original_export['xrefs_from']:
            if 'TicksToSeconds' in str(row['name']):c.hooks[int(row['address'],16)]=seconds
        for at,symbol in bundle.unresolved.items():
            if symbol=='QueryPerformanceCounter':c.hooks[at]=query
            elif symbol.startswith('?TicksToSeconds@'):c.hooks[at]=seconds
            elif symbol.startswith('?GetPriority@'):c.hooks[at]=priority
            elif symbol.startswith('?GetAffinity@'):c.hooks[at]=affinity
        c.setg(3,AUX);c.setg(4,OBJ);c.setg(5,AUX+0x100);c.setg(6,AUX+0x200)
        c.run(bundle.entry if compiled else 0x82BCC010)
        both.append((c.g(3)&0xffffffff,c.mem.rd(OBJ,0x600)+c.mem.rd(AUX,32)+c.mem.rd(AUX+0x1000,count*8)+c.mem.rd(AUX+0x2000,count*0x290),trace))
    mismatch('create-not-ready',index,both)
groups.append({'kind':'create-not-ready','cases':128});print('create-not-ready128 checked',flush=True)

report={'cases':sum(g['cases'] for g in groups),'groups':groups,'vtable_slots_checked':len(slots),'slots':slots,'mismatches':errors,
        'objects':{str(paths[name]):hashlib.sha256(paths[name].read_bytes()).hexdigest() for name in ['local_backend.cpp','job_instance_handle.cpp']},
        'scope':'actual XDK compiled functions against raw ARTIST,44 emitted vtable slots,5 genuine base default byte bodies,whole JobInstance Run and Initialize,8/24byte hidden return buffers,low-word u64 worker indices,forwarding strides and startup selection',
        'limits':'kernel,performance clock,entry callbacks,event playback,worker Start/Wait and Thread.GetId controlled;Initialize restricted to no semaphore and empty overflow chains;slot creation controls initialization,GC,tick conversion and watchdog;not physical concurrency/full scheduler execution'}
(OUT/'scheduler-abi-comparison.json').write_text(json.dumps(report,indent=2)+'\n')
print(report['cases'],'checks;',len(errors),'mismatches')
if errors:print(json.dumps(errors[:3],indent=2))
raise SystemExit(bool(errors))
