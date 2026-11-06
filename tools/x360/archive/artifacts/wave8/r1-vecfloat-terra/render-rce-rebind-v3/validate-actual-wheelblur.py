from pathlib import Path
import sys,random,json,hashlib
sys.path.insert(0,'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/scratch/x360_sdk');import validate_render_states as R
from validate_resource_pointer import Bundle
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave8/r1-vecfloat-terra');obj=out/'render-rce-rebind-v3/render/probe.obj';o=R.Coff(obj,'??__EgvWheelBlurConstants');bundle=Bundle([o]);bundle.select('??__EgvWheelBlurConstants');name=next(n for n in bundle.definitions if n.startswith('?gvWheelBlurConstants@'));oi,val,sec=bundle.definitions[name];dst=bundle.bases[(oi,sec)]+val;rng=random.Random(0x82c4b160);errors=[]
for i in range(64):
 before=rng.randbytes(16);poison=rng.randbytes(16);guard=rng.randbytes(4);results=[]
 for compiled in (False,True):
  c=bundle.cpu() if compiled else R.M.new_cpu();target=dst if compiled else 0x82fad6f0
  if compiled:c.mem.add(target-16,16);c.mem.add(target+16,4)
  else:c.mem.add(target-16,36)
  c.mem.wr(target-16,before);c.mem.wr(target,poison);c.mem.wr(target+16,guard);c.run(bundle.entry if compiled else 0x82c4b160)
  if c.mem.rd(target-16,16)!=before or c.mem.rd(target+16,4)!=guard:errors.append([i,'guard'])
  results.append(c.mem.rd(target,16))
 if results[0]!=results[1]:errors.append([i,results[0].hex(),results[1].hex()])
d={'cases':64,'mismatch_count':len(errors),'mismatches':errors[:2],'fixture_source_sha256':hashlib.sha256(Path('D:/Reverse/BP-X360-Experimental-Worktrees/r1_vecfloat_repair_terra/src/GameSource/World/EntityModules/RaceCarEntityModule/BrnRaceCarEntityModule_Render.cpp').read_bytes()).hexdigest(),'fixture_object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'actual Render.cpp emitted wheel blur initializer, CRT initializer vs ARTIST 82C4B160..B184 for 64 poisoned/guarded vectors; startup ordering and draw excluded'};(out/'render-rce-rebind-v3/actual-wheelblur-init-comparison.json').write_text(json.dumps(d,indent=2));print(json.dumps(d,indent=2));assert not errors
