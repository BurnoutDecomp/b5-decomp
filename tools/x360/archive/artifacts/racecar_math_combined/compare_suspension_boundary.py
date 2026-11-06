from pathlib import Path
import sys,random,struct,json,hashlib
sys.path.insert(0,'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/scratch/x360_sdk');import validate_render_states as R
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/racecar_math_combined');obj=out/'airram_provider/probe.obj';o=R.Coff(obj,'?UpdateSuspensionPostSimulation@VehiclePhysics@');rng=random.Random(639);errors=[]
for i in range(128):
 pos=rng.randbytes(16);imp=rng.randbytes(16);results=[]
 for compiled in (False,True):
  c=o.cpu() if compiled else R.M.new_cpu();sp=c.g(1);c.mem.wr(R.M.S.AUX,pos);c.setg(29,R.M.S.AUX);c.setg(27,0);c.setg(30,R.M.S.SELF);c.setg(31,R.M.S.SELF)
  if compiled:
   c.mem.wr(o.entry+0x6dc,bytes.fromhex('4e800020'));c.run(o.entry+0x6d8);c.mem.wr(sp+0xd0,imp);start=o.entry+0x7f8;end=o.entry+0x810
  else:
   image=(out/'825F6BB0.bin').read_bytes();c.mem.add(0x825f6bb0,len(image),image);c.mem.wr(sp+0xe0,imp);start=0x825f7620;end=0x825f7638
  c.mem.wr(end,bytes.fromhex('4e800020'));c.run(start);results.append((c.g(3),c.g(4),c.g(5),c.vr[1],c.vr[2]))
 if results[0]!=results[1]:errors.append({'case':i,'native':str(results[0]),'compiled':str(results[1])})
d={'cases':128,'mismatch_count':len(errors),'mismatches':errors[:5],'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'actual original 825F7620..7634 vs compiled UpdateSuspensionPostSimulation+7F8..80C call boundary; compiled position first loaded by its actual +6D8 lvx from 16 random source bytes including W, impulse seeded, checks r3/r4/r5 and v1/v2; earlier control path and callee controlled, no whole-function/hardware claim'};(out/'suspension-position-boundary-comparison.json').write_text(json.dumps(d,indent=2));print(json.dumps(d,indent=2));assert not errors
