import sys,random,struct,json,hashlib,math
from pathlib import Path
sys.path.insert(0,'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/scratch/x360_sdk');import validate_render_states as R
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/racecar_math_combined');obj=out/'airram_provider/probe.obj';o=R.Coff(obj,'?UpdateShunt@VehiclePhysics@');print(o.fn,o.unresolved)
rng=random.Random(636);errors=[]
for i in range(128):
 data=bytearray(rng.randbytes(0x2000));controls=bytearray(rng.randbytes(0x100));up=[rng.uniform(-1,1) for _ in range(3)];length=math.sqrt(sum(x*x for x in up));up=[x/length for x in up];vel=[rng.uniform(-2,2) for _ in range(3)];direction=[rng.uniform(-1,1) for _ in range(3)];mass=rng.uniform(500,3000);struct.pack_into('>4f',data,0x20,*up,rng.uniform(-1,1));struct.pack_into('>4f',data,0x50,*vel,rng.uniform(-1,1));struct.pack_into('>4f',data,0xe0,*([mass]*4));struct.pack_into('>4f',data,0x1130,*direction,rng.uniform(0.1,2.0));struct.pack_into('>4f',data,0x1140,1.0,100.0,0.0,0.0);data[0x135b]=1;struct.pack_into('>f',controls,4,rng.uniform(0.0,1.0));result=[]
 for compiled in (False,True):
  c=o.cpu() if compiled else R.M.new_cpu()
  for a,name in o.unresolved.items():
   if 'fpr_' in name:c.hooks[a]=lambda cpu:None
  trace=[]
  def impulse(cpu): trace.append((cpu.g(3),cpu.vr[1]))
  c.hooks[0x825be8f8]=impulse
  for a,name in o.unresolved.items():
   if 'AddWorldSpaceImpulse@' in name:c.hooks[a]=impulse
  c.mem.wr(R.M.S.SELF,data);c.mem.wr(R.M.S.AUX,controls);c.setg(3,R.M.S.SELF);c.setg(4,R.M.S.AUX);c.vr[1]=(0x3c888889,)*4;c.run(o.entry if compiled else 0x825fc748);result.append((c.mem.rd(R.M.S.SELF,0x2000),c.mem.rd(R.M.S.AUX,0x100),trace))
 if result[0]!=result[1]:errors.append({'case':i,'self_offsets':[j for j,(a,b) in enumerate(zip(result[0][0],result[1][0])) if a!=b][:20], 'native_trace':result[0][2], 'object_trace':result[1][2]})
d={'cases':128,'mismatch_count':len(errors),'mismatches':errors[:10],'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'whole actual UpdateShunt vs ARTIST 825FC748, random finite four-lane up/velocity, direction, mass and poisoned VehiclePhysics/controls, live traction impulse, callback arguments and full bytes compared; impulse provider controlled; hardware excluded'};(out/'shunt-live-comparison.json').write_text(json.dumps(d,indent=2));print(json.dumps(d,indent=2));assert not errors
