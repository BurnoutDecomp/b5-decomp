"""Execute actual compiled native string routines beside ARTIST with pool witnesses."""
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
OUT=ROOT/'build/x360/native-string-foundation'
rows=json.loads((OUT/'compile-results.json').read_text())['results']
paths={Path(r['source']).name:Path(r['object']) for r in rows if r['passed']}
bundle=Bundle([Coff(paths['EAString.cpp'],'?AllocateBuffer@EAStringC@@'),
               Coff(paths['AptStringPool.cpp'],'?Initialize@StringPool@@')])
identity=json.loads((ROOT.parent/'progress/identity.json').read_text())
OLD_ALLOC=0x82AE1058;OLD_FREE=0x82AE1308;OLD_EMPTY=0x82F72FF8
OLD_RECORDS=0x82F733FC;OLD_TABLE=0x8324E580
OBJ=M.S.SELF;AUX=M.S.AUX;OLD=AUX+0x1000;NEW=AUX+0x8000;POOL=AUX+0x18000
rng=random.Random(360_1029)


class StringCPU(CPU):
    def step(self,pc,word,nxt,depth):
        if word>>26==18 and not word&1:
            delta=word&0x03FFFFFC
            if delta&0x02000000:delta-=0x04000000
            target=(delta if word&2 else pc+delta)&0xFFFFFFFF
            if target in self.hooks:
                self.call(target,depth)
                raise CPU.run.__globals__['Return']()
        if word>>26==31:
            xo=(word>>1)&1023;reg=(word>>21)&31;ra=(word>>16)&31;rb=(word>>11)&31
            if xo==83: # mfmsr
                self.setg(reg,self.msr);return nxt
            if xo==178: # mtmsree: update only MSR[EE]
                self.msr=(self.msr&~0x8000)|(self.g(reg)&0x8000);return nxt
            if xo==20: # lwarx
                address=((self.g(ra) if ra else 0)+self.g(rb))&0xFFFFFFFF
                self.reservation=address;self.setg(reg,self.rd32(address));return nxt
            if xo==150: # stwcx.
                address=((self.g(ra) if ra else 0)+self.g(rb))&0xFFFFFFFF
                success=self.reservation==address and self.failures==0
                if self.failures:self.failures-=1
                if success:self.mem.wr(address,struct.pack('>I',self.g(reg)&0xFFFFFFFF))
                self.reservation=None;self.setcr(0,False,False,success);return nxt
        return super().step(pc,word,nxt,depth)


def loaded_address(fragment):
    matches=set()
    for oi,obj in enumerate(bundle.objects):
        for name,value,section in obj.symbols.values():
            if (name.startswith(fragment) if fragment.startswith('?') else fragment in name) and (oi,section) in bundle.bases:
                matches.add(bundle.bases[(oi,section)]+value)
    assert len(matches)==1,(fragment,matches)
    return matches.pop()


def fixture(compiled,allocations=True,failures=0):
    cpu=bundle.cpu();cpu.__class__=StringCPU;cpu.msr=0x8000;cpu.reservation=None;cpu.failures=failures
    cpu.mem.add(0x82F72000,0x9000);cpu.mem.add(0x8324D000,0x3000)
    cpu.mem.add(0x84800000,0x4000)
    trace=[]
    def allocate(c):
        assert allocations,'Allocation during pre-pool CRT initialization'
        trace.append(('alloc',c.g(3)&0xFFFFFFFF,c.g(4)&0xFFFFFFFF));c.setg(3,NEW)
    def free(c):
        assert allocations,'Free during pre-pool CRT initialization'
        trace.append(('free',c.g(3)&0xFFFFFFFF,c.g(4)&0xFFFFFFFF,c.g(5)&0xFFFFFFFF));c.setg(3,1)
    def copy(c):c.mem.wr(c.g(3)&0xFFFFFFFF,c.mem.rd(c.g(4)&0xFFFFFFFF,c.g(5)&0xFFFFFFFF))
    def fill(c):c.mem.wr(c.g(3)&0xFFFFFFFF,bytes([c.g(4)&255])*(c.g(5)&0xFFFFFFFF))
    def length(c):
        start=c.g(3)&0xFFFFFFFF;length=0
        while c.mem.rd(start+length,1)!=b'\0':length+=1;assert length<0x10000
        c.setg(3,length)
    def strcpy(c):
        start=c.g(4)&0xFFFFFFFF;length=0
        while c.mem.rd(start+length,1)!=b'\0':length+=1;assert length<0x10000
        c.mem.wr(c.g(3)&0xFFFFFFFF,c.mem.rd(start,length+1))
    cpu.hooks[OLD_ALLOC]=allocate;cpu.hooks[OLD_FREE]=free
    funcs={'memcpy':copy,'memmove':copy,'memset':fill,'strlen':length,'strcpy':strcpy}
    for name,fn in funcs.items():
        for address in identity.get(name,{}).get('x360_addrs',[]):cpu.hooks[int(address,16)]=fn
    for at,name in bundle.unresolved.items():
        if 'gpNonGCPoolManager' in name:cpu.mem.wr(at,struct.pack('>I',POOL if allocations else 0))
        elif name.startswith('?Allocate@DOGMA_PoolManager@@'):cpu.hooks[at]=allocate
        elif name.startswith('?Deallocate@DOGMA_PoolManager@@'):cpu.hooks[at]=free
        elif name in funcs:cpu.hooks[at]=funcs[name]
        elif name in ['atexit','_atexit']:cpu.hooks[at]=lambda c:c.setg(3,0)
        elif 'gpValuesToRelease' in name:cpu.mem.wr(at,b'\0'*4)
    cpu.mem.wr(0x8324D808,struct.pack('>I',POOL if allocations else 0))
    return cpu,trace


errors=[];groups=[]
specs=[('allocate','?AllocateBuffer@EAStringC@@',0x82AE3438),
       ('free','?DeallocateBuffer@EAStringC@@',0x82AE19C8),
       ('decrease','?DecreaseInternalRefCount@EAStringC@@',0x82AE3358),
       ('assign','??4EAStringC@@QAAAAV0@ABV0@@Z',0x82AE33B8),
       ('change','?ChangeBuffer@EAStringC@@',0x82AE34A0)]
for kind,prefix,original in specs:
    bundle.select(prefix)
    empty=loaded_address('s_EmptyInternalData@EAStringC') if kind in ['decrease','assign','change'] else 0
    count=256 if kind=='change' else 128
    for index in range(count):
        own=bytearray(rng.randbytes(0x100));data=bytearray(rng.randbytes(0x10000))
        ref=[0,1,2,3,0x7FFF,0xFFFE,0xFFFF][index%7]
        struct.pack_into('>4H',data,0x1000,ref,128,192,rng.randrange(65536))
        struct.pack_into('>4H',data,0x2000,ref,128,192,rng.randrange(65536))
        size=[0,1,2,3,4,7,8,64,127,255,0xFFFF,0xFFFFFFF4][index%12]
        mode=index%6;offset=index%9;copy_size=index%40;internal=copy_size
        if kind=='change':
            ref=1 if mode in [0,1,5] else 2
            capacity=256 if mode in [0,1,5] else 16
            size=64 if mode in [0,1,2,3,5] else 0
            struct.pack_into('>4H',data,0x1000,ref,128,capacity,rng.randrange(65536))
        outputs=[]
        for compiled in [False,True]:
            cpu,trace=fixture(compiled,failures=index%3 if kind in ['decrease','assign'] else 0)
            cpu.mem.wr(OBJ,own);cpu.mem.wr(AUX,data)
            sentinel=empty if compiled else OLD_EMPTY
            cpu.mem.wr(OBJ,struct.pack('>II',OLD,OLD+0x1000))
            if kind=='allocate':cpu.setg(3,OBJ);cpu.setg(4,size)
            elif kind=='free':cpu.setg(3,OLD);cpu.setg(4,size)
            elif kind=='decrease':cpu.setg(3,sentinel if index%8==0 else OLD)
            elif kind=='assign':
                dest=OBJ;source=OBJ if mode==0 else OBJ+4
                if mode==1:cpu.mem.wr(OBJ+4,struct.pack('>I',sentinel))
                if mode==2:cpu.mem.wr(OBJ,struct.pack('>I',sentinel))
                if mode==3:cpu.mem.wr(OBJ+4,struct.pack('>I',OLD))
                cpu.setg(3,dest);cpu.setg(4,source)
            elif kind=='change':
                cpu.setg(3,OBJ);cpu.setg(4,size);cpu.setg(5,offset);cpu.setg(6,copy_size)
                cpu.setg(7,index%2);cpu.setg(8,internal)
            cpu.run(bundle.entry if compiled else original)
            state=bytearray(cpu.mem.rd(OBJ,0x100))
            for at in [0,4]:
                if struct.unpack_from('>I',state,at)[0]==sentinel:struct.pack_into('>I',state,at,OLD_EMPTY)
            outputs.append((bytes(state),cpu.mem.rd(AUX,0x10000),trace,cpu.msr,
                            (cpu.g(3)&0xFFFFFFFF) if kind=='assign' else None))
        if outputs[0]!=outputs[1]:errors.append({'kind':kind,'case':index,'trace':[o[2] for o in outputs],
            'object_differences':[i for i,(a,b) in enumerate(zip(outputs[0][0],outputs[1][0])) if a!=b],
            'data_differences':[i for i,(a,b) in enumerate(zip(outputs[0][1],outputs[1][1])) if a!=b]})
    groups.append({'kind':kind,'cases':count});print(kind,count,'checked',flush=True)

# Execute ARTIST's initializer over its original image data. Record0 already
# has its header and text in that image; the initializer fills the other87.
bundle.select('?Initialize@StringPool@@')
records=loaded_address('sStringPoolData')
original_cpu,_=fixture(False)
original_cpu.mem.wr(OLD_RECORDS,M.V.x360rd.rd(OLD_RECORDS,88*264))
original_cpu.run(0x82C71F10)
expected=original_cpu.mem.rd(OLD_RECORDS,88*264)
compiled_cpu,_=fixture(True)
actual=compiled_cpu.mem.rd(records,88*264)
if actual!=expected:errors.append({'kind':'static-records','bytes':[i for i,(a,b) in enumerate(zip(actual,expected)) if a!=b]})
groups.append({'kind':'static-records','cases':88})
print('static records88 checked',flush=True)

# The default EAString array initializer must not reach the uninitialized pool.
crt_prefix='??__E?saConstant@StringPool@@'
bundle.select(crt_prefix)
cpu,trace=fixture(True,allocations=False)
cpu.run(bundle.entry)
table=loaded_address('?saConstant@StringPool@@');empty=loaded_address('s_EmptyInternalData@EAStringC')
assert cpu.mem.rd(table,88*4)==struct.pack('>I',empty)*88
assert not trace
groups.append({'kind':'pre-pool-array-CRT','cases':1})
print('pre-pool CRT checked',flush=True)

for kind,prefix,original in [('initialize','?Initialize@StringPool@@',0x82AE3630),
                             ('teardown','?Teardown@StringPool@@',0x82AE3720)]:
    bundle.select(prefix)
    table=loaded_address('?saConstant@StringPool@@')
    empty=loaded_address('s_EmptyInternalData@EAStringC') if kind=='teardown' else 0
    record_base=loaded_address('sStringPoolData') if kind=='initialize' else 0
    buckets=loaded_address('gpAptStringPoolBuckets');nitems=loaded_address('gnAptStringPoolCount')
    lock=loaded_address('gStringPoolTableLock')
    for count in [0,1,2,4,16,64]:
        outputs=[]
        for compiled in [False,True]:
            c,trace=fixture(compiled)
            tab=table if compiled else OLD_TABLE;rec=record_base if compiled else OLD_RECORDS
            sentinel=empty if compiled else OLD_EMPTY
            c.mem.wr(NEW,b'\0'*(count*4))
            c.mem.wr(tab,struct.pack('>I',sentinel)*88 if kind=='initialize' else struct.pack('>88I',*(OLD_RECORDS+i*264 for i in range(88))))
            if kind=='teardown':
                c.mem.wr(buckets if compiled else 0x8324E4F4,struct.pack('>I',NEW))
                c.mem.wr(nitems if compiled else 0x8324E4F8,struct.pack('>I',count))
            c.setg(3,count);c.run(bundle.entry if compiled else original)
            pointers=struct.unpack('>88I',c.mem.rd(tab,352))
            normalized=tuple(v-rec if kind=='initialize' else (0 if v==sentinel else v) for v in pointers)
            outputs.append((normalized,c.mem.rd(NEW,count*4),trace,
                            c.rd32(nitems if compiled else 0x8324E4F8),
                            c.rd32(lock if compiled else 0x8324E7D4),c.msr))
        if outputs[0]!=outputs[1]:errors.append({'kind':kind,'count':count,'outputs':outputs})
    groups.append({'kind':kind,'cases':6});print(kind,'6 checked',flush=True)

result={'groups':groups,'cases':sum(g['cases'] for g in groups),'mismatches':errors,
        'objects':{k:str(v) for k,v in paths.items()},
        'scope':'actual native COFF vs ARTIST;complete memory,allocation/free argument and MSR witnesses;0/1/2 reservation failures;88 exact static records;pre-pool CRT without allocations;pool initialize/empty-bucket teardown',
        'limits':'controlled pool callbacks,not whole DOGMA allocator/game/hardware concurrency or complete EAString suite;teardown live-node walks unchanged and not exercised'}
(OUT/'comparison.json').write_text(json.dumps(result,indent=2,default=lambda x:x.hex() if isinstance(x,bytes) else str(x))+'\n')
print(result['cases'],'native string checks;',len(errors),'mismatches')
if errors:print(json.dumps(errors[:2],indent=2,default=str)[:4500])
raise SystemExit(bool(errors))
