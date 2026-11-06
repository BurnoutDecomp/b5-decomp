from pathlib import Path
import sys,random,struct,json,hashlib
sys.path.insert(0,'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/scratch/x360_sdk');import validate_render_states as R
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave8/r1-vecfloat-terra');obj=out/'ActiveRaceCarRenderParams/probe.obj';o=R.Coff(obj,'?DEBUG_OverrideScratchAmount@RenderParams@');rng=random.Random(0x822a21b0);errors=[]
for i in range(128):
 before=rng.randbytes(0x1500);bits=struct.unpack('>Q',struct.pack('>d',rng.uniform(-100,100)))[0];results=[]
 for compiled in (False,True):
  c=o.cpu() if compiled else R.M.new_cpu();c.mem.wr(R.M.S.SELF,before);c.setg(3,R.M.S.SELF);c.fbits[1]=bits
  if not compiled:
   image=(out/'822A21B0.bin').read_bytes();c.mem.add(0x822a21b0,len(image),image)
  c.run(o.entry if compiled else 0x822a21b0);results.append(c.mem.rd(R.M.S.SELF,0x1500))
 if results[0]!=results[1]:errors.append({'case':i,'offsets':[j for j,(a,b) in enumerate(zip(*results)) if a!=b][:12]})
d={'cases':128,'mismatch_count':len(errors),'mismatches':errors[:5],'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'whole actual RenderParams::DEBUG_OverrideScratchAmount vs original 822A21B0, 128 finite scratch amounts, random raw 0x1500-byte RenderParams including 128 four-lane offsets; original RenderRaceCar branch call and GPU excluded'};(out/'renderparams-scratch-mutation-comparison.json').write_text(json.dumps(d,indent=2));print(json.dumps(d,indent=2));assert not errors
