from pathlib import Path
import sys,random,hashlib,json
sys.path.insert(0,'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/scratch/x360_sdk');import validate_render_states as R
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave8/racecar_consumers');obj=out/'BrnLionBlendRenderer/probe.obj';o=R.Coff(obj,'?SetCameraData@LionBlendRenderer@');rng=random.Random(822824);errors=[]
for i in range(128):
 data=rng.randbytes(0x400);back=rng.randbytes(64);view=rng.randbytes(64);proj=rng.randbytes(64);results=[]
 for compiled in (False,True):
  c=o.cpu() if compiled else R.M.new_cpu();a=R.M.S.SELF;b=R.M.S.AUX;c.mem.wr(a,data);c.mem.wr(b,back+view+proj);c.setg(3,a);c.setg(4,b);c.setg(5,b+64);c.setg(6,b+128)
  try:c.run(o.entry if compiled else 0x822824f8)
  except Exception as e:print('case',i,'compiled',compiled,type(e).__name__,e);raise
  results.append((c.mem.rd(a,0x400),c.mem.rd(b,192)))
 if results[0]!=results[1]:errors.append({'case':i,'offsets':[j for j,(x,y) in enumerate(zip(results[0][0],results[1][0])) if x!=y][:12]})
d={'cases':128,'mismatch_count':len(errors),'mismatches':errors[:8],'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'whole actual SetCameraData vs ARTIST 822824F8, random raw 64-byte input rows including W, full 0x400 destination and inputs checked; no render/GPU claim'};(out/'lion-camera-comparison.json').write_text(json.dumps(d,indent=2));print(json.dumps(d,indent=2));assert not errors
