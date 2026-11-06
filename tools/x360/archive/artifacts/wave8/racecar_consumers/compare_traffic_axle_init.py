from pathlib import Path
import sys,random,json,hashlib,struct
sys.path.insert(0,'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/scratch/x360_sdk');import validate_render_states as R
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave8/racecar_consumers');obj=out/'traffic_type_probe/probe.obj';o=R.Coff(obj,'?axle@@');rng=random.Random(0x8275);errors=[]
for i in range(256):
 before=rng.randbytes(0x100);c=o.cpu();c.mem.wr(R.M.S.SELF,before);c.setg(3,R.M.S.SELF+32);c.run(o.entry);expected=before[:32]+bytes(16)+struct.pack('>4I',0,0x3f800000,0,0)+before[64:];actual=c.mem.rd(R.M.S.SELF,0x100)
 if actual!=expected:errors.append(i)
d={'cases':256,'mismatch_count':len(errors),'mismatches':errors[:5],'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'actual Axle::Initialise header wrapper against existing zero-position and up-Y 16-byte data contract, full poisoned object and guards; original individual inline instruction trace and whole vehicle lifecycle not compared'};(out/'traffic-axle-init-comparison.json').write_text(json.dumps(d,indent=2));print(json.dumps(d,indent=2));assert not errors
