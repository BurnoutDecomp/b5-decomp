import sys,random,struct,json,hashlib
from pathlib import Path
sys.path.insert(0,'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/scratch/x360_sdk');import validate_render_states as R
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/racecar_math_combined');obj=out/'airram_provider/probe.obj';o=R.Coff(obj,'?UpdateShunt@VehiclePhysics@');print(o.fn,o.unresolved)
rng=random.Random(636);errors=[]
for i in range(128):
 data=bytearray(rng.randbytes(0x2000));controls=rng.randbytes(0x100);v=0.0 if i%2==0 else 2.0;struct.pack_into('>f',data,0x113c,v);struct.pack_into('>f',data,0x1140,1.0);data[0x135b]=0;result=[]
 for compiled in (False,True):
  c=o.cpu() if compiled else R.M.new_cpu()
  for a,name in o.unresolved.items():
   if 'fpr_' in name:c.hooks[a]=lambda cpu:None
  c.mem.wr(R.M.S.SELF,data);c.mem.wr(R.M.S.AUX,controls);c.setg(3,R.M.S.SELF);c.setg(4,R.M.S.AUX);c.vr[1]=(0x3c888889,)*4;c.run(o.entry if compiled else 0x825fc748);result.append((c.mem.rd(R.M.S.SELF,0x2000),c.mem.rd(R.M.S.AUX,0x100)))
 if result[0]!=result[1]:errors.append({'case':i,'self_offsets':[j for j,(a,b) in enumerate(zip(result[0][0],result[1][0])) if a!=b][:20]})
d={'cases':128,'mismatch_count':len(errors),'mismatches':errors[:10],'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'whole actual UpdateShunt vs ARTIST 825FC748, random poisoned VehiclePhysics/controls, inactive and active-no-traction clear, preserves other bytes, FPR save helper controlled; traction/live-impulse paths and hardware excluded'};(out/'shunt-clear-comparison.json').write_text(json.dumps(d,indent=2));print(json.dumps(d,indent=2));assert not errors
