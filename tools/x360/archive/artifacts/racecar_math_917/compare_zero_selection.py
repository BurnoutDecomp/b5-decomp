import sys,random,json,hashlib
from pathlib import Path
sys.path.insert(0,'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/scratch/x360_sdk');import validate_render_states as R
base=Path('D:/Reverse/BP-X360-Experimental-Artifacts/racecar_math_917');p=base/'physics/probe.obj';rng=random.Random(9170);errs=[]; specs=[('?UpdateTargetAssist@RaceCarPhysics@',0x8261ff50,0x826200c4,0x826200c8,0x134,0x154,11,12,11,13),('?UpdateTargetAssist@RaceCarPhysics@',0x8261ff50,0x82620238,0x8262023c,0x34c,0x364,126,7,12,126),('?UpdateAftertouch@RaceCarPhysics@',0x8262ebe8,0x8262f2c8,0x8262f2cc,0x464,0x480,3,10,13,0)]
for si,(sym,origin,start,end,cs,ce,reg,zreg,vreg,maskreg) in enumerate(specs):
 o=R.Coff(p,sym)
 for i in range(128):
  vals=[]
  for compiled in (False,True):
   c=o.cpu() if compiled else R.M.new_cpu()
   if not compiled:
    data=(base/('%08X.bin'%origin)).read_bytes();c.mem.add(origin,len(data),data)
   begin=o.entry+cs if compiled else start;finish=o.entry+ce if compiled else end;c.mem.wr(finish,bytes.fromhex('4e800020'));c.setf(11,0.0);c.setf(29,0.0);c.setg(27,0);c.vr[zreg]=(0,0,0,0);c.vr[vreg]=tuple(rng.getrandbits(32) for _ in range(4));c.vr[maskreg]=(0,0,0,0);c.run(begin);vals.append(c.vr[0] if compiled else c.vr[reg])
  if vals[0]!=vals[1]:errs.append((si,i,vals))
d={'cases_per_slice':128,'mismatches':errs,'object_sha256':hashlib.sha256(p.read_bytes()).hexdigest(),'original_slices':['826200C4','82620238','8262F2C8'],'compiled_slices':['UpdateTargetAssist+134..150','UpdateTargetAssist+34C..360','UpdateAftertouch+464..47C'],'scope':'actual instruction slices, prior native zero selection and registers seeded; compare zero-result four lanes with poisoned nonselected inputs; not full normalize, branch reachability or caller/callee behavior'};(base/'zero-selection-slices-comparison.json').write_text(json.dumps(d,indent=2));print(d);assert not errs

