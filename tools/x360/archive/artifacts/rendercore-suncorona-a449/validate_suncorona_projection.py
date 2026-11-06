from pathlib import Path
import hashlib
import json
import math
import random
import struct
import sys

sys.path.insert(0,r'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/scratch/x360_sdk')
import native_atomic_fixture as F

out=Path(r'D:/Reverse/BP-X360-Experimental-Artifacts/rendercore-suncorona-a449')
row=next(r for r in json.loads((out/'native-provider1/compile-results.json').read_text())['results'] if Path(r['source']).name=='BrnSunCorona.cpp')
path=Path(row['object']);image=F.Coff(path,'?ComputeSunPositionOnScreen@BrnSunCorona@')
SELF,AUX=F.M.S.SELF,F.M.S.AUX
SUN,MATRIX=SELF+0x1000,AUX+0x1000
rng=random.Random(0x823fd798);errors=[]

for index in range(256):
    sun=bytearray(rng.randbytes(0x80))
    multiplier=[0.0,-0.0,0.5,-1.0,2.0,math.nan][index%6] if index<48 else rng.uniform(-3.0,3.0)
    struct.pack_into('>f',sun,0x34,multiplier)
    view=[rng.uniform(-10000.0,10000.0) for _ in range(3)]+[0.0]
    direction=[rng.uniform(-4.0,4.0) for _ in range(3)]+[0.0]
    matrix=[rng.uniform(-2.0,2.0) for _ in range(16)]
    if index%16==0:direction=[0.0,0.0,0.0,0.0]
    if index%16==1:direction=[0.0,1.0,0.0,0.0]
    if index%16==2:matrix=[0.0]*16
    if index%16==3:matrix=[1.0 if n%5==0 else 0.0 for n in range(16)]
    if index%16==4:direction[1]=math.nan
    view_bytes=struct.pack('>4f',*view);direction_bytes=struct.pack('>4f',*direction);matrix_bytes=struct.pack('>16f',*matrix)
    both=[]
    for compiled in (False,True):
        c=F.cpu(image);c.mem.wr(SUN,sun);c.mem.wr(MATRIX,matrix_bytes)
        c.mem.wr((c.g(1)&0xffffffff)-0x2000,bytes([index&255])*0x2000)
        c.setg(3,SUN);c.setg(4,MATRIX)
        if compiled:
            # Frozen shared POD Vector3 uses paired GPRs; this fixture supplies
            # equal typed input values and makes no retail by-value ABI claim.
            for first,data in [(5,view_bytes),(7,direction_bytes)]:
                c.setg(first,int.from_bytes(data[:8],'big'));c.setg(first+1,int.from_bytes(data[8:],'big'))
        else:
            c.vr[1]=struct.unpack('>4I',view_bytes);c.vr[2]=struct.unpack('>4I',direction_bytes)
        c.run(image.entry if compiled else 0x823fd798)
        both.append((c.mem.rd(SUN,0x80),c.mem.rd(MATRIX,64)))
    if both[0]!=both[1]:
        errors.append(dict(case=index,differences=[hex(n) for n,(a,b) in enumerate(zip(both[0][0],both[1][0])) if a!=b],
                           original=both[0][0][0x48:0x54].hex(),compiled=both[1][0][0x48:0x54].hex(),
                           multiplier=struct.pack('>f',multiplier).hex(),direction=direction_bytes.hex(),matrix=matrix_bytes.hex()))

result=dict(cases=256,errors=errors,object=str(path),object_sha256=hashlib.sha256(path.read_bytes()).hexdigest(),
    scope='Actual complete Sun ComputeSunPositionOnScreen versus ARTIST823FD798; native SDK VMX dot/estimate/two Newton refinements, fused matrix projection, reciprocal refinements and screen/visibility stores execute without service hooks. All Sun/matrix bytes compare, including degenerate/zero/NaN cases.',
    limits='The frozen shared POD Vector3 entry ABI is supplied through paired GPRs for the compiled function versus native v1/v2. This is equal-value body/dataflow proof, not retail shared VPU ABI or physical hardware estimate validation.')
report=out/'native-provider1/sun-projection-comparison.json'
assert not report.exists(),'Refusing to overwrite completed evidence'
report.write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps(dict(cases=256,errors=len(errors),first_error=errors[:2]),indent=2))
raise SystemExit(bool(errors))
