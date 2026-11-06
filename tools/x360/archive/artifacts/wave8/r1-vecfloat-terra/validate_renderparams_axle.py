from pathlib import Path
import sys,random,json,hashlib
sys.path.insert(0,'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/scratch/x360_sdk');import validate_render_states as R
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave8/r1-vecfloat-terra');obj=out/'ActiveRaceCarRenderParams/probe.obj';o=R.Coff(obj,'?GetAxlePosition@RenderParams@');rng=random.Random(0x822e7ed8);errors=[]
for i in range(128):
 idx=i%4;before=rng.randbytes(0x1500);c=o.cpu();c.mem.wr(R.M.S.SELF,before);c.setg(3,R.M.S.SELF);c.setg(4,idx);c.run(o.entry)
 if c.g(3)!=R.M.S.SELF+0xb40+idx*16 or c.mem.rd(R.M.S.SELF,0x1500)!=before:errors.append(i)
d={'cases':128,'mismatch_count':len(errors),'mismatches':errors[:5],'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'actual const GetAxlePosition pointer result/unchanged params against DWARF pointer signature and ARTIST 822E7ED8/7EE0 axle0/3 addresses +0xB40/0xB70, 16-byte stride; indices 1/2 derive from declared array, no shadow/GPU claim'};(out/'renderparams-axle-getter-comparison.json').write_text(json.dumps(d,indent=2));print(json.dumps(d,indent=2));assert not errors
