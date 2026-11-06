from pathlib import Path
import sys,random,struct,json,hashlib
sys.path.insert(0,'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/scratch/x360_sdk');import validate_render_states as R
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave8/racecar_consumers');obj=out/'BrnPhysicalTrafficManager/probe.obj';o=R.Coff(obj,'?AddArticulatedJointContacts@PhysicalTrafficManager@');rng=random.Random(0x825f);errors=[]
for i in range(256):
 data=rng.randbytes(16);results=[]
 for compiled in (False,True):
  c=o.cpu() if compiled else R.M.new_cpu();c.mem.wr(R.M.S.AUX,data);c.vr[0]=(0x80000000,)*4
  if compiled:c.setg(10,R.M.S.AUX);c.setg(24,0);c.setg(11,R.M.S.AUX+0x100);start=o.entry+0x30c;end=o.entry+0x31c
  else:
   image=(out/'825F3000.bin').read_bytes();c.mem.add(0x825f3000,len(image),image);c.setg(11,R.M.S.AUX);c.setg(15,0);start=0x825f3428;end=0x825f3434
  c.mem.wr(end,bytes.fromhex('4e800020'));c.run(start);results.append(c.vr[0])
 if results[0]!=results[1]:errors.append({'case':i,'native':str(results[0]),'compiled':str(results[1])})
d={'cases':256,'mismatch_count':len(errors),'mismatches':errors[:5],'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'actual articulated-contact axis lvx/vxor slice vs ARTIST 825F3428..3430; random raw four-lane input including W and NaN bit patterns, seeded sign mask, full vector output; prior path/callee and hardware excluded'};(out/'traffic-joint-sign-comparison.json').write_text(json.dumps(d,indent=2));print(json.dumps(d,indent=2));assert not errors
