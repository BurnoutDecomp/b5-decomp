from pathlib import Path
import sys,random,hashlib,json
sys.path.insert(0,'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/scratch/x360_sdk');import validate_render_states as R
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave8/racecar_consumers');obj=out/'LionParticleRender/probe.obj';o=R.Coff(obj,'?SetCameraData@LionParticleRender@');rng=random.Random(822810);errors=[]
for i in range(128):
 data=rng.randbytes(0x400);matrices=rng.randbytes(256);results=[]
 for compiled in (False,True):
  c=o.cpu() if compiled else R.M.new_cpu();a=R.M.S.SELF;b=R.M.S.AUX;c.mem.wr(a,data);c.mem.wr(b,matrices);c.setg(3,a)
  for j in range(4):c.setg(4+j,b+64*j)
  try:c.run(o.entry if compiled else 0x82281068)
  except Exception as e:print('error',compiled,type(e).__name__,e);raise
  results.append((c.mem.rd(a,0x400),c.mem.rd(b,256)))
 if results[0]!=results[1]:errors.append({'case':i,'offsets':[j for j,(x,y) in enumerate(zip(results[0][0],results[1][0])) if x!=y][:12]})
d={'cases':128,'mismatch_count':len(errors),'mismatches':errors[:8],'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'whole actual LionParticleRender::SetCameraData vs ARTIST 82281068, random raw four 64-byte input matrices including W, full 0x400 destination and input bytes checked; no GPU claim'};(out/'lion-particle-camera-comparison.json').write_text(json.dumps(d,indent=2));print(json.dumps(d,indent=2));assert not errors
