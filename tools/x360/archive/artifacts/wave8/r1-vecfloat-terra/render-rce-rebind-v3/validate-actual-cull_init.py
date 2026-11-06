from pathlib import Path
import sys,random,json,hashlib
sys.path.insert(0,'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/scratch/x360_sdk');import validate_render_states as R
from validate_resource_pointer import Bundle
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave8/r1-vecfloat-terra');obj=out/'render-rce-rebind-v3/render/probe.obj';o=R.Coff(obj,'??__EgavShadowCullDistances');bundle=Bundle([o]);bundle.select('??__EgavShadowCullDistances');_,val,sec=next(x for x in o.symbols.values() if x[0]=='gavShadowCullDistances');dst=bundle.bases[(0,sec)]+val;rng=random.Random(0x82c4bcb8);errors=[]
for i in range(64):
 before=rng.randbytes(16);poison=rng.randbytes(48);guard=rng.randbytes(4);results=[]
 for compiled in (False,True):
  c=bundle.cpu() if compiled else R.M.new_cpu();target=dst if compiled else 0x82fad4a0
  if compiled:c.mem.add(target-16,16);c.mem.add(target+48,4)
  else:c.mem.add(target-16,68)
  c.mem.wr(target-16,before);c.mem.wr(target,poison);c.mem.wr(target+48,guard);c.run(bundle.entry if compiled else 0x82c4bcb8)
  if c.mem.rd(target-16,16)!=before or c.mem.rd(target+48,4)!=guard:errors.append([i,'guard'])
  results.append(c.mem.rd(target,48))
 if results[0]!=results[1]:errors.append([i,results[0].hex(),results[1].hex()])
d={'cases':64,'mismatch_count':len(errors),'mismatches':errors[:5],'fixture_source_sha256':hashlib.sha256(Path('D:/Reverse/BP-X360-Experimental-Worktrees/r1_vecfloat_repair_terra/src/GameSource/World/EntityModules/RaceCarEntityModule/BrnRaceCarEntityModule_Render.cpp').read_bytes()).hexdigest(),'fixture_object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'actual Render.cpp emitted cull initializer; native compiler CRT array initializer vs ARTIST 82C4BCB8..BD1C, 64 randomized poison/guards; 3x16 table bytes and initializer effects only, program init ordering and gameplay excluded'};(out/'render-rce-rebind-v3/actual-cull-init-comparison.json').write_text(json.dumps(d,indent=2));print(json.dumps(d,indent=2));assert not errors
