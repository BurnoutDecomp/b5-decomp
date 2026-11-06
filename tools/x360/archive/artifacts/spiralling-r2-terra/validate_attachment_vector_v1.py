from pathlib import Path
import json,random,struct,sys,hashlib
P=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(P/'scratch/x360_sdk'));import native_atomic_fixture as F
O=Path('D:/Reverse/BP-X360-Experimental-Artifacts/spiralling-r2-terra');j=json.loads((O/'representative-v1/compile-results.json').read_text());obj=Path(j['results'][0]['object']);sym='?Update@BehaviourSpirallingDeathcam@';b=F.Bundle([F.Coff(obj,sym)]);b.select(sym);CODE=F.M.S.AUX+0x5000

def run(compiled,car,old,scalar):
 c=F.cpu(b);c.mem.add(CODE,0x1000)
 if compiled:words=[int.from_bytes(c.mem.rd(b.entry+off,4),'big') for off in [0x158,0x180]];c.vr[12]=scalar;out=0
 else:words=[F.M.V.x360rd.u32(pc) for pc in [0x8224a7f4,0x8224a814]];c.vr[127]=scalar;out=3
 c.vr[13]=car;c.vr[0]=old;c.mem.wr(CODE,b''.join(struct.pack('>I',x) for x in words)+struct.pack('>I',0x4e800020));c.run(CODE);return tuple(c.vr[out])
rng=random.Random(0x8224a814)
for i in range(256):
 vals=struct.unpack('>9I',struct.pack('>9f',*[rng.uniform(-1e3,1e3) for _ in range(9)]));car=vals[:4];old=vals[4:8];scalar=(vals[8],)*4;a=run(False,car,old,scalar);z=run(True,car,old,scalar)
 if a!=z:raise AssertionError((i,a,z))
r={'cases':256,'scope':'ARTIST 8224A7F4/814 vsubfp plus vmaddcfp128 vs actual Update vsubfp/vmaddfp; full four raw output lanes for finite randomized vectors and splatted coefficient, VMX emulator.','object':str(obj),'sha256':hashlib.sha256(obj.read_bytes()).hexdigest()};(O/'attachment-vector-comparison-v1.json').write_text(json.dumps(r,indent=2)+'\n');print(json.dumps(r,indent=2))
