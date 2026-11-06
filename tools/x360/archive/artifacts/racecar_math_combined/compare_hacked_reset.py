from pathlib import Path
import sys,random,struct,json,hashlib
sys.path.insert(0,'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/scratch/x360_sdk');import validate_render_states as R
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/racecar_math_combined');obj=out/'airram_provider/probe.obj';o=R.Coff(obj,'?HackedResetAndFlyAround@VehiclePhysics@');rng=random.Random(637);errors=[]
for i in range(128):
 data=bytearray(rng.randbytes(0x2000));mat=[1.,0.,0.,0.,0.,1.,0.,0.,0.,0.,1.,0.,rng.uniform(-100,100),rng.uniform(-100,100),rng.uniform(-100,100),1.];struct.pack_into('>16f',data,0x10,*mat);controls=bytes(0x100);result=[]
 for compiled in (False,True):
  c=o.cpu() if compiled else R.M.new_cpu();c.mem.wr(R.M.S.SELF,data);c.mem.wr(R.M.S.AUX,controls);c.setg(3,R.M.S.SELF);c.setg(4,R.M.S.AUX);c.vr[1]=(0x3c888889,)*4;c.run(o.entry if compiled else 0x825d0008);result.append((c.mem.rd(R.M.S.SELF,0x2000),c.mem.rd(R.M.S.AUX,0x100)))
 if result[0]!=result[1]:errors.append({'case':i,'self_offsets':[j for j,(a,b) in enumerate(zip(result[0][0],result[1][0])) if a!=b][:20]})
d={'cases':128,'mismatch_count':len(errors),'mismatches':errors[:10],'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'whole actual HackedResetAndFlyAround vs ARTIST 825D0008; random poisoned VehiclePhysics, identity basis, finite random position, zero controls, all 0x2000 vehicle and controls compared; other transforms/controls and hardware excluded'};(out/'hacked-reset-comparison.json').write_text(json.dumps(d,indent=2));print(json.dumps(d,indent=2));assert not errors
