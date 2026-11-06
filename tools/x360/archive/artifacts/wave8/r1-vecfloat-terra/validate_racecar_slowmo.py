from pathlib import Path
import sys,random,struct,json,hashlib
sys.path.insert(0,'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/scratch/x360_sdk');import validate_render_states as R
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave8/r1-vecfloat-terra');obj=out/'RaceCarPhysics/probe.obj';o=R.Coff(obj,'?Update@RaceCarPhysics@');rng=random.Random(0x826416d8);errors=[]
for i in range(256):
 dt=tuple(rng.getrandbits(32) for _ in range(4));results=[]
 for compiled in (False,True):
  c=o.cpu() if compiled else R.M.new_cpu();sp=c.g(1)
  if compiled:
   c.vr[127]=dt;c.setg(3,R.M.S.AUX);c.mem.wr(R.M.S.AUX,struct.pack('>4I',*([0x3c23d70a]*4)));start=o.entry+0xb0;end=o.entry+0xc0
  else:
   image=(out/'826415E8.bin').read_bytes();c.mem.add(0x826415e8,len(image),image);c.vr[0]=(0x42c80000,)*4;c.vr[13]=(0x3f800000,)*4;c.vr[1]=dt;start=0x826416bc;end=0x826416dc
  c.mem.wr(end,bytes.fromhex('4e800020'));c.run(start);results.append(c.vr[127])
 if results[0]!=results[1]:errors.append([i,results[0],results[1]])
d={'cases':256,'mismatch_count':len(errors),'mismatches':errors[:3],'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'actual RaceCarPhysics::Update slowmo VMX multiply vs ARTIST 826416BC..D8 reciprocal/refinement/multiply for native factor 100, arbitrary raw four-lane timestep, scale constant seeded as actual constructor output; software estimate model, other Update logic excluded'};(out/'racecar-slowmo-lanes.json').write_text(json.dumps(d,indent=2));print(json.dumps(d,indent=2));assert not errors

