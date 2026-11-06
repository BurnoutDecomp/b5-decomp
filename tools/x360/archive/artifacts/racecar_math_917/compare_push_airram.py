import sys,random,json,hashlib
from pathlib import Path
sys.path.insert(0,'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/scratch/x360_sdk');import validate_render_states as R
base=Path('D:/Reverse/BP-X360-Experimental-Artifacts/racecar_math_917');p=base/'physics/probe.obj';o=R.Coff(p,'?UpdateShowtimePhysics@RaceCarPhysics@');rng=random.Random(9179);errors=[]
for i in range(128):
 results=[]
 for compiled in (False,True):
  c=o.cpu() if compiled else R.M.new_cpu()
  if not compiled:
   data=(base/'825FFBD8.bin').read_bytes();c.mem.add(0x825ffbd8,len(data),data)
  start=o.entry+0x404 if compiled else 0x8260004c;end=o.entry+0x44c if compiled else 0x82600058
  c.mem.wr(end,bytes.fromhex('4e800020'));c.setg(27,R.M.S.SELF);c.setg(28,R.M.S.AUX+0x100);c.setg(30,0);c.setg(11,R.M.S.AUX+0x100);c.setg(10,0x20);c.setf(3,0.0);c.setf(30,1.0);c.vr[0]=(1,1,1,1);c.vr[2]=tuple(rng.getrandbits(32) for _ in range(4));c.run(start);results.append(c.vr[2])
 if results[0]!=results[1]:errors.append((i,results))
d={'cases':128,'mismatches':errors,'object_sha256':hashlib.sha256(p.read_bytes()).hexdigest(),'original_slice':'8260004C..82600054','compiled_offsets':'UpdateShowtimePhysics+0x404..0x448','scope':'actual original and object v2 before normalization/call; zero native scalar/GPR inputs and arbitrary poisoned incoming v2, prior path controlled; no v1, full caller or callee claim'};(base/'push-airram-v2-comparison.json').write_text(json.dumps(d,indent=2));print(d);assert not errors
