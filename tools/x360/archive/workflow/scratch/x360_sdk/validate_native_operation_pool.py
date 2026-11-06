"""Compare restored native file-operation storage and scheduler with ARTIST."""
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
OUT=ROOT/'build/x360/native-thread-sdk-repair'
rows=json.loads((OUT/'compile-results.json').read_text())['results']
path=Path(next(r['object'] for r in rows if r['source'].endswith('CgsDeviceOperationPool.cpp') and r['passed']))
bundle=Bundle([Coff(path,'?AddOperation@OperationPool@')])
tree=ast.parse((Path(__file__).parent/'validate_native_strings.py').read_text())
nodes=[n for n in tree.body if isinstance(n,(ast.ClassDef,ast.FunctionDef)) and n.name in ['StringCPU','loaded_address']]
exec(compile(ast.Module(body=nodes,type_ignores=[]),'reservation-fixture','exec'),globals())
identity=json.loads((ROOT.parent/'progress/identity.json').read_text())
rng=random.Random(360_1030_4)
OBJ=M.S.SELF;AUX=M.S.AUX
errors=[]
for prefix,address,kind in [('?AddOperation@OperationPool@',0x828E9828,'post'),
                            ('?ReadOperation@OperationPool@',0x828E9960,'read')]:
    bundle.select(prefix)
    for index in range(128):
        image=bytearray(rng.randbytes(0x5000));data=rng.randbytes(0x130)
        used_count=[0,1,2,32,63,64][index%6]
        if kind=='read' and not used_count:used_count=1
        used=set(rng.sample(range(64),used_count))
        image[0x20:0x60]=bytes(int(i in used) for i in range(64))
        for slot in range(64):
            number=[0,1,2,0x100000000,0xFFFFFFFFFFFFFFFE][(index+slot)%5]
            priority=[-1,0,1,10][(slot+index)%4]
            struct.pack_into('>Q',image,0x60+slot*8,number)
            struct.pack_into('>i',image,0x260+slot*0x130+0x12C,priority)
        struct.pack_into('>Q',image,0x4E60,(index<<32)|1234)
        both=[]
        for compiled in [False,True]:
            c=bundle.cpu();c.__class__=StringCPU;c.msr=0x8000;c.reservation=None;c.failures=0
            c.mem.add(0x84800000,0x4000);c.mem.wr(OBJ,image);c.mem.wr(AUX,data)
            trace=[]
            def enter(x):trace.append(('enter',x.g(3)&0xFFFFFFFF))
            def leave(x):trace.append(('leave',x.g(3)&0xFFFFFFFF))
            def sempost(x):trace.append(('post',x.g(3)&0xFFFFFFFF,x.g(4)&0xFFFFFFFF));x.setg(3,1)
            def semwait(x):trace.append(('wait',x.g(3)&0xFFFFFFFF,x.rd32(x.g(4)&0xFFFFFFFF)));x.setg(3,0)
            def copy(x):
                dest,source,count=[x.g(i)&0xFFFFFFFF for i in [3,4,5]]
                x.mem.wr(dest,x.mem.rd(source,count));x.setg(3,dest)
            callbacks={'RtlEnterCriticalSection':enter,'RtlLeaveCriticalSection':leave,'memcpy':copy}
            for name,fn in callbacks.items():
                for old in identity.get(name,{}).get('x360_addrs',[]):c.hooks[int(old,16)]=fn
            c.hooks[0x82B42B70]=sempost;c.hooks[0x82B42A40]=semwait
            for at,name in bundle.unresolved.items():
                if name in callbacks:c.hooks[at]=callbacks[name]
                elif name.startswith('?Post@Semaphore@'):c.hooks[at]=sempost
                elif name.startswith('?Wait@Semaphore@'):c.hooks[at]=semwait
            c.setg(3,OBJ);c.setg(4,AUX)
            c.run(bundle.entry if compiled else address)
            both.append((c.g(3)&0xFFFFFFFF,c.mem.rd(OBJ,0x5000),c.mem.rd(AUX,0x130),trace,c.msr))
        if both[0]!=both[1]:errors.append({'kind':kind,'index':index,'return':[o[0] for o in both],
                                         'trace':[o[3] for o in both],
                                         'bytes':[(i,a,b) for i,(a,b) in enumerate(zip(both[0][1],both[1][1])) if a!=b][:16]})
    print(kind,'128 checked',flush=True)
report={'cases':256,'mismatches':errors,'object':str(path),'object_sha256':hashlib.sha256(path.read_bytes()).hexdigest(),
        'scope':'complete actual native pool AddOperation/ReadOperation vs ARTIST; full5000hex pool bytes,130hex payload,return andOS/semaphore arguments;64slots,empty andfull posts,priority selection and64bit FIFOties; native offset/stride compile gates',
        'limits':'kernel/semaphore/memcpy callbacks controlled; no physical IO/concurrency,empty-pop assert or sequence-exhaustion assert path claim'}
(OUT/'operation-pool-comparison.json').write_text(json.dumps(report,indent=2)+'\n')
print('operation pool256 checks;',len(errors),'mismatches')
if errors:print(json.dumps(errors[:3],indent=2))
raise SystemExit(bool(errors))
