from pathlib import Path
import sys,random,json,hashlib
sys.path.insert(0,'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/scratch/x360_sdk');import validate_render_states as R
from validate_resource_pointer import Bundle
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave8/racecar_consumers');pa=out/'BrnEffectsUtils/probe.obj';pb=out/'vecfloat/probe.obj';rng=random.Random(0x82c4a0);errors=[]
for name,addr,dst_original in [('K_VECFLOAT_255',0x82c4a0b0,0x82fac220),('K_VECTOR4_511_511_511_255',0x82c4a0d8,0x82fac210),('K_VECFLOAT_ONEOVER255',0x82c4a110,0x82fac100)]:
 a=R.Coff(pa,'??__E'+name+'@');b=R.Coff(pb,'??0VecFloat@vpu@math@rw@@QAA@ABM@');bundle=Bundle([a,b]);bundle.select('??__E'+name+'@');n=next(n for n in bundle.definitions if n.startswith('?'+name+'@'));oi,val,sec=bundle.definitions[n];dst=bundle.bases[(oi,sec)]+val
 for i in range(64):
  poison=rng.randbytes(16);guard=rng.randbytes(4);before=rng.randbytes(16);results=[]
  for compiled in (False,True):
   c=bundle.cpu() if compiled else R.M.new_cpu();target=dst if compiled else dst_original
   if not compiled:c.mem.add(target-16,36)
   else:
    c.mem.add(target-16,16)
    try:c.mem.rd(target+16,4)
    except KeyError:c.mem.add(target+16,4)
   c.mem.wr(target-16,before);c.mem.wr(target,poison);c.mem.wr(target+16,guard);c.run(bundle.entry if compiled else addr)
   if c.mem.rd(target-16,16)!=before or c.mem.rd(target+16,4)!=guard:errors.append([name,i,'guard'])
   results.append(c.mem.rd(target,16))
  if results[0]!=results[1]:errors.append([name,i,results[0].hex(),results[1].hex()])
d={'cases_per_initializer':64,'mismatch_count':len(errors),'mismatches':errors[:5],'object_sha256':hashlib.sha256(pa.read_bytes()).hexdigest(),'vecfloat_sha256':hashlib.sha256(pb.read_bytes()).hexdigest(),'scope':'three actual CRT initializers vs original 82C4A0B0/0D8/110, real linked SDK VecFloat constructor, destination poison and end guards; initialization ordering not tested'};(out/'effects-constants-crt-comparison.json').write_text(json.dumps(d,indent=2));print(json.dumps(d,indent=2));assert not errors


