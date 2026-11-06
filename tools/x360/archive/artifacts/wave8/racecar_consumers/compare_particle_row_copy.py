from pathlib import Path
import sys,random,json,hashlib
sys.path.insert(0,'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/scratch/x360_sdk');import validate_render_states as R
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave8/racecar_consumers');obj=out/'ParticleModule/probe.obj';o=R.Coff(obj,'?RowCopyToAffine@BrnParticle@@');rng=random.Random(0x82294760);errors=[]
for i in range(256):
 c=o.cpu();src=rng.randbytes(64);poison=rng.randbytes(128);c.mem.wr(R.M.S.SELF,src);c.mem.wr(R.M.S.AUX,poison);c.setg(3,R.M.S.AUX+32);c.setg(4,R.M.S.SELF);c.run(o.entry);expected=poison[:32]+src+poison[96:];got=c.mem.rd(R.M.S.AUX,128)
 if got!=expected:errors.append(i)
d={'cases':256,'mismatch_count':len(errors),'mismatches':errors[:5],'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'actual RowCopyToAffine full helper versus original four-VMX-row-copy data contract in PreRenderUpdate 82294760, random raw 64-byte matrix including special floats and poisoned surrounding bytes; no camera/GPU claim'};(out/'particle-row-copy-comparison.json').write_text(json.dumps(d,indent=2));print(json.dumps(d,indent=2));assert not errors
