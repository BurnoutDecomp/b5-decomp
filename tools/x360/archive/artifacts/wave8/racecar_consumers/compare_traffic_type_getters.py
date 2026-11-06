from pathlib import Path
import sys,random,struct,json,hashlib
sys.path.insert(0,'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/scratch/x360_sdk');import validate_render_states as R
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave8/racecar_consumers');obj=out/'traffic_type_probe/probe.obj';rng=random.Random(0x82756738);errors=[]
for name,offset,lane in [('cap',0x20,0),('trailer',0x20,1),('back',0x20,2),('forward',0x20,3),('mass',0x30,0),('radius',0x30,1)]:
 o=R.Coff(obj,'?'+name+'@@')
 for i in range(256):
  raw=rng.randbytes(0x100);c=o.cpu();c.mem.wr(R.M.S.SELF,raw);c.setg(3,R.M.S.SELF);c.run(o.entry);value=struct.unpack_from('>I',raw,offset+lane*4)[0]
  if c.vr[1]!=(value,)*4 or c.mem.rd(R.M.S.SELF,0x100)!=raw:errors.append([name,i])
d={'cases_per_accessor':256,'mismatch_count':len(errors),'mismatches':errors[:5],'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'actual header getter wrappers, 6 genuine VecFloat values in v1, lane-index and raw-bit splat contract attested by DWARF and original VehicleAxles 82756738, random full source bytes including special floats; no whole traffic behavior claim'};(out/'traffic-type-getter-abi-comparison.json').write_text(json.dumps(d,indent=2));print(json.dumps(d,indent=2));assert not errors
