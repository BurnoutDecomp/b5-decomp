from pathlib import Path
import sys,random,json,hashlib
sys.path.insert(0,'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/scratch/x360_sdk');import validate_render_states as R
from validate_resource_pointer import Bundle
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave8/racecar_consumers');pa=out/'BrnPhysicalTrafficManager/probe.obj';pb=out/'vecfloat/probe.obj';a=R.Coff(pa,'??__EKVF_RESET_ON_WATER_HEIGHT@');b=R.Coff(pb,'??0VecFloat@vpu@math@rw@@QAA@ABM@');bundle=Bundle([a,b]);bundle.select('??__EKVF_RESET_ON_WATER_HEIGHT@');name=next(n for n in bundle.definitions if n.startswith('?KVF_RESET_ON_WATER_HEIGHT@'));oi,val,sec=bundle.definitions[name];dst=bundle.bases[(oi,sec)]+val;base=bundle.bases[(oi,sec)];rng=random.Random(2508);errors=[]
for i in range(64):
 poison=rng.randbytes(16);guard=rng.randbytes(4);results=[]
 for compiled in (False,True):
  c=bundle.cpu() if compiled else R.M.new_cpu();target=dst if compiled else 0x82fb92d0
  if not compiled:c.mem.add(target-16,36)
  else:c.mem.add(target-16,16)
  before=rng.randbytes(16);c.mem.wr(target-16,before);c.mem.wr(target,poison);c.mem.wr(target+16,guard);c.run(bundle.entry if compiled else 0x82c5b808);assert c.mem.rd(target-16,16)==before and c.mem.rd(target+16,4)==guard;results.append(c.mem.rd(target,16))
 if results[0]!=results[1]:errors.append({'case':i,'native':results[0].hex(),'compiled':results[1].hex()})
d={'cases':64,'mismatch_count':len(errors),'mismatches':errors[:5],'traffic_object_sha256':hashlib.sha256(pa.read_bytes()).hexdigest(),'vecfloat_object_sha256':hashlib.sha256(pb.read_bytes()).hexdigest(),'scope':'original 82C5B808 CRT initializer vs actual linked Traffic initializer plus actual SDK VecFloat constructor; poisoned 16-byte destination, 16-byte prior and 4-byte following guards; initializer ordering not tested'};(out/'traffic-water-constant-comparison.json').write_text(json.dumps(d,indent=2));print(json.dumps(d,indent=2));assert not errors
