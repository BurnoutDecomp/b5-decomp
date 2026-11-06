import sys,random,json,hashlib
from pathlib import Path
sys.path.insert(0,'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/scratch/x360_sdk');import validate_render_states as R
base=Path('D:/Reverse/BP-X360-Experimental-Artifacts/racecar_math_917');p=base/'physics/probe.obj';o=R.Coff(p,'?UpdateShowtimePhysics@RaceCarPhysics@');rng=random.Random(9178);errors=[]
for i in range(128):
 v=tuple(rng.getrandbits(32) for _ in range(4));results=[]
 for compiled in (False,True):
  c=o.cpu() if compiled else R.M.new_cpu()
  if not compiled:
   data=(base/'825FFBD8.bin').read_bytes();c.mem.add(0x825ffbd8,len(data),data)
  start=o.entry+0x388 if compiled else 0x825fffd8;end=o.entry+0x3b4 if compiled else 0x825ffff4
  c.mem.wr(end,bytes.fromhex('4e800020'));c.setg(27,0x40000000);c.setg(28,R.M.S.AUX+0x100);c.setg(30,0);c.setf(3,0.0);c.setf(30,0.0);c.setf(31,1.0);c.vr[127]=v;c.vr[2]=tuple(rng.getrandbits(32) for _ in range(4));c.run(start);results.append((c.vr[1],c.vr[2]))
 if results[0]!=results[1]:errors.append((i,results))
d={'cases':128,'mismatches':errors,'object_sha256':hashlib.sha256(p.read_bytes()).hexdigest(),'original_slice':'825FFFD8..825FFFF0','compiled_offsets':'UpdateShowtimePhysics+0x388..0x3B0','scope':'actual native and object outgoing v1/v2; arbitrary incoming v127, poisoned v2 and native zero scalar/register inputs; prior path and AddAirRam controlled, no full caller/callee claim'};(base/'bounce-airram-registers-comparison.json').write_text(json.dumps(d,indent=2));print(d);assert not errors
