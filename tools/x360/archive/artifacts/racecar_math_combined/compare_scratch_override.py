from pathlib import Path
import sys,random,struct,json,hashlib
sys.path.insert(0,'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/scratch/x360_sdk');import validate_render_states as R
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/racecar_math_combined');obj=out/'params/probe.obj';o=R.Coff(obj,'?DEBUG_OverrideScratchAmount@RenderParams@');rng=random.Random(126);errors=[]
for i in range(128):
 data=rng.randbytes(0x14a0);v=[0.0,-0.0,1.0,-1.0,float('inf'),float('-inf'),float('nan')][i] if i<7 else struct.unpack('>f',struct.pack('>f',rng.uniform(-100,100)))[0];result=[]
 for compiled in (False,True):
  c=o.cpu() if compiled else R.M.new_cpu();c.mem.wr(R.M.S.SELF,data);c.setg(3,R.M.S.SELF);c.setf(1,v);c.run(o.entry if compiled else 0x822a21b0);result.append((c.g(3),c.mem.rd(R.M.S.SELF,0x14a0)))
 if result[0]!=result[1]:errors.append({'case':i,'offsets':[j for j,(a,b) in enumerate(zip(result[0][1],result[1][1])) if a!=b][:10]})
d={'cases':128,'mismatch_count':len(errors),'mismatches':errors[:5],'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'whole actual DEBUG_OverrideScratchAmount vs ARTIST 822A21B0; all 0x14A0 RenderParams bytes and return register, random raw initial values and finite/special scratch values; no caller/GPU claim'};(out/'scratch-override-comparison.json').write_text(json.dumps(d,indent=2));print(json.dumps(d,indent=2));assert not errors
