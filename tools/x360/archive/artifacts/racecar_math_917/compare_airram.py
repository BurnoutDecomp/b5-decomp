import sys,random,json,hashlib
from pathlib import Path
sys.path.insert(0,'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/scratch/x360_sdk');import validate_render_states as R
base=Path('D:/Reverse/BP-X360-Experimental-Artifacts/racecar_math_917');p=base/'physics/probe.obj';o=R.Coff(p,'?SetPlayerVehicleInShowtime@RaceCarPhysics@');rng=random.Random(917);errors=[]
for i in range(128):
 results=[]
 for compiled in (False,True):
  c=o.cpu() if compiled else R.M.new_cpu()
  if not compiled:
   data=(base/'826000F8.bin').read_bytes();c.mem.add(0x826000f8,len(data),data)
  start=o.entry+0x254 if compiled else 0x8260049c;end=o.entry+0x2a4 if compiled else 0x826004c4
  c.mem.wr(end,bytes.fromhex('4e800020'))
  # Scalar flag arithmetic is unrelated to the vector arguments and controlled in this slice.
  for at in ([o.entry+0x260,o.entry+0x270] if compiled else [0x826004a8,0x826004b8]):c.mem.wr(at,bytes.fromhex('60000000'))
  c.setg(26,0);c.setg(27,0x82f2a2c8);c.setg(28,R.M.S.AUX+0x100);c.setg(30,0x40000000);c.setg(11,1);c.setf(31,0.0);c.vr[126]=(0,0,0,0)
  for k in [1,2]:c.vr[k]=tuple(rng.getrandbits(32) for _ in range(4))
  c.run(start);results.append((c.vr[1],c.vr[2]))
 if results[0]!=results[1]:errors.append((i,results))
d={'cases':128,'mismatches':errors,'object_sha256':hashlib.sha256(p.read_bytes()).hexdigest(),'original_slice':'8260049C..826004C0','compiled_offsets':'SetPlayerVehicleInShowtime+0x254..0x2A0','scope':'actual original and object instruction slices through outgoing v1/v2, seeded prior native zero v126, zero f31 and r26, arbitrary poisoned incoming vectors; scalar flag arithmetic controlled; not full caller or AddAirRam execution'};(base/'airram-zero-registers-comparison.json').write_text(json.dumps(d,indent=2));print(d);assert not errors

