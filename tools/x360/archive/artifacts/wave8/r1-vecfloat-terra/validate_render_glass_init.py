from pathlib import Path
import sys,random,json,hashlib
sys.path.insert(0,'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/scratch/x360_sdk');import validate_render_states as R
from validate_resource_pointer import Bundle
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave8/r1-vecfloat-terra');obj=out/'render_glass_init/probe.obj';o=R.Coff(obj,'??__EKAV4_GLASS_FRACTURE_UV_OFFSETS');bundle=Bundle([o]);bundle.select('??__EKAV4_GLASS_FRACTURE_UV_OFFSETS');_,val,sec=next(x for x in o.symbols.values() if x[0]=='KAV4_GLASS_FRACTURE_UV_OFFSETS');dst=bundle.bases[(0,sec)]+val;rng=random.Random(0x82c4bd40);errors=[]
for i in range(64):
 before=rng.randbytes(16);poison=rng.randbytes(128);guard=rng.randbytes(4);results=[]
 for compiled in (False,True):
  c=bundle.cpu() if compiled else R.M.new_cpu();target=dst if compiled else 0x82fad420
  if compiled:c.mem.add(target-16,16);c.mem.add(target+128,4)
  else:c.mem.add(target-16,148)
  c.mem.wr(target-16,before);c.mem.wr(target,poison);c.mem.wr(target+128,guard);c.run(bundle.entry if compiled else 0x82c4bd40)
  if c.mem.rd(target-16,16)!=before or c.mem.rd(target+128,4)!=guard:errors.append([i,'guard'])
  results.append(c.mem.rd(target,128))
 if results[0]!=results[1]:errors.append([i,results[0].hex(),results[1].hex()])
d={'cases':64,'mismatch_count':len(errors),'mismatches':errors[:2],'fixture_source_sha256':hashlib.sha256((out/'render-glass-init-fixture.cpp').read_bytes()).hexdigest(),'fixture_object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'isolated fixture copied exact current Render.cpp glass UV offset table and compiled CRT initializer vs ARTIST 82C4BD40..BEBC, 64 poisoned/guarded runs, 128 table bytes including negative zero; actual Render CPP and init ordering excluded'};(out/'render-glass-init-fixture-comparison.json').write_text(json.dumps(d,indent=2));print(json.dumps(d,indent=2));assert not errors
