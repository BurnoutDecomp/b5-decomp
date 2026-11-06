from pathlib import Path
import hashlib
import json
import random
import struct
import sys

sys.path.insert(0, r'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/scratch/x360_sdk')
import native_atomic_fixture as F

out = Path(r'D:/Reverse/BP-X360-Experimental-Artifacts/rendercore-suncorona-a449')
paths = {Path(r['source']).name:Path(r['object']) for r in json.loads((out/'native-provider1/compile-results.json').read_text())['results'] if r['passed']}
bundle = F.Bundle([F.Coff(paths['BrnSunCorona.cpp'], '?Destruct@BrnSunCorona@'),
                   F.Coff(paths['vertexdescriptor.cpp'], '?Release@VertexDescriptor@'),
                   F.Coff(paths['device.cpp'], '?sub_82B61F90@')])
bundle.select('?Destruct@BrnSunCorona@')
SELF, AUX = F.M.S.SELF, F.M.S.AUX
SUN, DATA, ALLOC_A, ALLOC_B, VTABLE = [SELF+n for n in [0x1000,0x2000,0x3000,0x3100,0x4000]]
FREE, DEVICE = 0x84803000, AUX+0x1000
gp_compiled = F.address(bundle, '?gpD3DDevice@')
rng=random.Random(0x823f8298)
errors=[]

for index in range(128):
    sun=bytearray(rng.randbytes(0x80))
    data=bytearray(rng.randbytes(0x80))
    null_allocator=index%8==1
    declaration=0 if index%4==0 else AUX+0x2000+index*4
    struct.pack_into('>I',sun,0,0 if null_allocator else ALLOC_A)
    struct.pack_into('>5I',sun,4,DATA,0x11112222,index,0x33334444,0xffffffff-index)
    struct.pack_into('>I',sun,0x18,DATA)
    struct.pack_into('>I',data,0,declaration)
    both=[]
    for compiled in (False,True):
        c=F.cpu(bundle)
        c.mem.wr(SUN,sun);c.mem.wr(DATA,data)
        c.mem.add(0x83271608,4)
        gp=gp_compiled if compiled else 0x83271608
        F.put(c,gp,DEVICE)
        for a in [ALLOC_A,ALLOC_B]:F.put(c,a,VTABLE)
        c.mem.wr(VTABLE,bytes(0x30));F.put(c,VTABLE+0x14,FREE)
        c.setg(3,SUN);trace=[]
        def context():return(c.mem.rd(SUN,0x80).hex(),c.mem.rd(DATA,0x80).hex(),c.mem.rd(gp,4).hex())
        def begin(cpu):trace.append(('assert_begin',context()))
        def fire(cpu):trace.append(('assert_fire',F.cstring(cpu,cpu.g(3)&0xffffffff),F.cstring(cpu,cpu.g(4)&0xffffffff),cpu.g(5)&0xffffffff,context()))
        def end(cpu):
            trace.append(('assert_end',context()))
            if null_allocator:F.put(cpu,SUN,ALLOC_A)
        def is_set(cpu):
            trace.append(('IsSet',cpu.g(3)&0xffffffff,cpu.g(4)&0xffffffff,context()))
            F.put(cpu,gp,DEVICE+0x100)
            cpu.setg(3,index%3)
        def unset(cpu):trace.append(('SetVertexDeclaration',cpu.g(3)&0xffffffff,cpu.g(4)&0xffffffff,context()))
        def release(cpu):
            trace.append(('Release',cpu.g(3)&0xffffffff,context()))
            if index&1:F.put(cpu,SUN,ALLOC_B)
            if index&2:F.put(cpu,SUN+0x18,DATA+0x40)
            F.put(cpu,DATA,0xfedcba98)
            cpu.setg(3,7)
        def free(cpu):
            trace.append(('Free',cpu.g(3)&0xffffffff,(cpu.g(4)&0xffffffff)-SUN,
                          cpu.mem.rd(cpu.g(4)&0xffffffff,20).hex(),context()))
            assert cpu.g(3)&0xffffffff in [ALLOC_A,ALLOC_B]
            c.mem.wr(DATA,bytes([0xdd])*0x80)
        c.hooks.update({0x82817548:begin,0x82820810:fire,0x82817558:end,
                        0x82942118:is_set,0x82943c80:unset,0x82941dc8:release,FREE:free})
        for at,name in bundle.unresolved.items():
            if name.startswith('?BeginAssert@'):c.hooks[at]=begin
            elif name.startswith('?FireAssert@'):c.hooks[at]=fire
            elif name.startswith('?EndAssert@'):c.hooks[at]=end
            elif name=='D3DResource_IsSet':c.hooks[at]=is_set
            elif name=='D3DDevice_SetVertexDeclaration':c.hooks[at]=unset
            elif name=='D3DResource_Release':c.hooks[at]=release
        c.run(bundle.entry if compiled else 0x823f8298)
        both.append((c.mem.rd(SUN,0x80),c.mem.rd(DATA,0x80),c.mem.rd(gp,4),trace))
    if both[0]!=both[1]:errors.append(dict(case=index,traces=[value[-1] for value in both]))

result=dict(cases=128,errors=errors,
    objects={n:dict(path=str(p),sha256=hashlib.sha256(p.read_bytes()).hexdigest()) for n,p in paths.items()},
    scope='Whole Sun Destruct823F8298 with actual compiled void VertexDescriptor::Release/sub_82B61F90. Original assert text/file/line/continuation, descriptor and captured-device SDK calls, five-lane DoFree receiver/member identity and all Sun/resource bytes compare, including callback-mutated allocator/member pointers.',
    limits='SDK IsSet/SetVertexDeclaration/Release, allocator DoFree and assertion continuation are controlled; actual Sun/provider/helper bodies execute. No full-link/physical GPU claim.')
report=out/'native-provider1/sun-destruct-comparison.json'
assert not report.exists(),'Refusing to overwrite completed evidence'
report.write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps(dict(cases=128,errors=len(errors),first_error=errors[:1]),indent=2))
raise SystemExit(bool(errors))
