"""Compare native timer and semaphore changes with the complete original routines."""
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
OUT=ROOT/'build/x360/native-timing-inspection-v2'
rows=json.loads((OUT/'compile-results.json').read_text())['results']
paths={Path(r['source']).name:Path(r['object']) for r in rows if r['passed']}
bundle=Bundle([Coff(paths['jobs.cpp'],'?TicksToSeconds@Jobs@EA@@'),
               Coff(paths['eathread_semaphore.cpp'],'?Wait@Semaphore@Thread@EA@@')])
# Reuse only the tested reservation/MSR/tail-call fixture class, without running
# that module's string tests or mutating any production source.
tree=ast.parse((Path(__file__).parent/'validate_native_strings.py').read_text())
nodes=[n for n in tree.body if isinstance(n,(ast.ClassDef,ast.FunctionDef)) and n.name in ['StringCPU','loaded_address']]
exec(compile(ast.Module(body=nodes,type_ignores=[]),'string-fixture-definitions','exec'),globals())


class TimingCPU(StringCPU):
    def step(self,pc,word,nxt,depth):
        if word>>26==31 and ((word>>1)&1023)==27: # sld RA,RS,RB
            source=(word>>21)&31;dest=(word>>16)&31;shift=self.g((word>>11)&31)&127
            value=(self.g(source)<<shift)&0xFFFFFFFFFFFFFFFF if shift<64 else 0
            self.setg(dest,value)
            if word&1:self.setcr(0,bool(value>>63),bool(value and not value>>63),value==0)
            return nxt
        if word>>26==31 and ((word>>1)&1023)==58: # cntlzd RA,RS
            source=(word>>21)&31;dest=(word>>16)&31
            count=64-(self.g(source)&0xFFFFFFFFFFFFFFFF).bit_length()
            self.setg(dest,count)
            if word&1:self.setcr(0,False,count>0,count==0)
            return nxt
        return super().step(pc,word,nxt,depth)


OLD_CLOCK=0x8327F288
rng=random.Random(360_1029_2)
errors=[];groups=[]
helper_evidence=ROOT/'build/x360/native-timing-inspection-v2/sdk-u64tod-comparison.json'
helper=json.loads(helper_evidence.read_text())
assert helper['byte_match'] and helper['code_bytes']==40
assert hashlib.sha256(Path(helper['library']).read_bytes()).hexdigest()==helper['library_sha256']
assert M.V.x360rd.rd(0x82C0C670,40).hex()==helper['artist_hex']


def cpu(now=0,status=0):
    c=bundle.cpu();c.__class__=TimingCPU;c.msr=0x8000;c.reservation=None;c.failures=0
    c.mem.add(0x8327F200,0x200);c.mem.add(0x84800000,0x4000)
    trace=[]
    def tick(x):trace.append(('ticks',now));x.setg(3,now)
    def wait(x):trace.append(('wait',x.g(3)&0xFFFFFFFF,x.g(4)&0xFFFFFFFF));x.setg(3,status)
    def convert(x):x.run(0x82C0C670)
    c.hooks[0x82929AA8]=tick;c.hooks[0x8292C808]=wait
    for at,name in bundle.unresolved.items():
        if name=='GetTickCount':c.hooks[at]=tick
        elif name=='WaitForSingleObject':c.hooks[at]=wait
        elif name=='__u64tod':c.hooks[at]=convert
    return c,trace


bundle.select('?TicksToSeconds@Jobs@EA@@')
clock=loaded_address('ldSecondsPerTick')
inputs=[0,1,2,0x2F90838,0xFFFFFFFF,0x100000000,0x7FFFFFFFFFFFFFFF,
        0x8000000000000000,0xFFFFFFFFFFFFFFFF]+[rng.getrandbits(64) for _ in range(247)]
for value in inputs:
    c,_=cpu();c.setg(3,value);c.run(0x82C0C670)
    if value:
        leading=64-value.bit_length()
        expected=((1086-leading)<<52)|(((value<<leading)>>11)&((1<<52)-1))
    else:expected=0
    assert struct.unpack('>Q',struct.pack('>d',c.f(1)))[0]==expected
for initial in [0.0,-0.0,1.0/49875000.0,0.5]:
    for ticks in inputs:
        outputs=[]
        for compiled in [False,True]:
            c,trace=cpu();target=clock if compiled else OLD_CLOCK
            c.mem.wr(target,struct.pack('>d',initial));c.setg(3,ticks)
            c.run(bundle.entry if compiled else 0x82BC9988)
            outputs.append((struct.pack('>f',c.f(1)),c.mem.rd(target,8),trace))
        if outputs[0]!=outputs[1]:errors.append({'kind':'ticks','input':hex(ticks),'initial':initial,
                                                'bits':[o[0].hex() for o in outputs],
                                                'clock':[o[1].hex() for o in outputs]})
groups.append({'kind':'ticks','cases':len(inputs)*4});print('ticks',len(inputs)*4,'checked',flush=True)

bundle.select('?Wait@Semaphore@Thread@EA@@')
statuses=[0,1,0x80,0x102,0x103,0xFFFFFFFF,0xFFFFFFFE,0x80000000]+[rng.getrandbits(32) for _ in range(248)]
for index,status in enumerate(statuses):
    for now in [0,0xFFFFFF00]:
        deadlines=[0,0xFFFFFFFF,(now+9999)&0xFFFFFFFF,(now+10000)&0xFFFFFFFF,
                   (now+10001)&0xFFFFFFFF,0x12345678]
        deadline=deadlines[index%len(deadlines)]
        count=[-1,0,1,2,123][index%5]
        own=bytearray(rng.randbytes(0x100));struct.pack_into('>IiiB',own,0,0x12345678,count,0,0)
        outputs=[]
        for compiled in [False,True]:
            c,trace=cpu(now,status);c.mem.wr(M.S.SELF,own)
            c.mem.wr(M.S.AUX,struct.pack('>I',deadline));c.setg(3,M.S.SELF);c.setg(4,M.S.AUX)
            c.run(bundle.entry if compiled else 0x82B42A40)
            outputs.append((c.g(3)&0xFFFFFFFF,c.mem.rd(M.S.SELF,0x100),trace,c.msr))
        if outputs[0]!=outputs[1]:errors.append({'kind':'semaphore','status':hex(status),'now':hex(now),
                                               'deadline':hex(deadline),'return':[o[0] for o in outputs],
                                               'trace':[o[2] for o in outputs]})
groups.append({'kind':'interprocess-wait','cases':len(statuses)*2});print('semaphore',len(statuses)*2,'checked',flush=True)
result={'cases':sum(g['cases'] for g in groups),'groups':groups,'mismatches':errors,
        'objects':{k:str(v) for k,v in paths.items()},
        'sdk_helper_evidence':str(helper_evidence),'helper_model_reference_checks':len(inputs),
        'scope':'actual compiled candidate functions vs ARTIST;native original __u64tod instructions executed;float32 result/cached float64;full64-bit tick inputs;cold/warm caches;OS wait/tick callbacks controlled;timeout,success and random failure statuses;memory/MSR/argument witnesses',
        'limits':'ignored candidate only;not production yet;no hardware timing/concurrency;unchanged intra-process semaphore arm not exercised;FPSCR rounding modes not varied'}
(OUT/'comparison.json').write_text(json.dumps(result,indent=2)+'\n')
print(result['cases'],'timing checks;',len(errors),'mismatches')
if errors:print(json.dumps(errors[:3],indent=2))
raise SystemExit(bool(errors))
