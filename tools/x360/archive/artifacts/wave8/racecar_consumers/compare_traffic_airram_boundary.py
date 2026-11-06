from pathlib import Path
import sys,random,json,hashlib
sys.path.insert(0,'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/scratch/x360_sdk');import validate_render_states as R
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave8/racecar_consumers');obj=out/'BrnPhysicalTrafficManager/probe.obj';o=R.Coff(obj,'?ProcessAddAirRamEvent@PhysicalTrafficManager@');rng=random.Random(0x8261);errors=[]
for i in range(256):
 event=rng.randbytes(64);results=[]
 for compiled in (False,True):
  c=o.cpu() if compiled else R.M.new_cpu();c.mem.wr(R.M.S.AUX,event)
  if compiled:c.setg(31,R.M.S.AUX);c.setg(28,R.M.S.SELF);first=o.entry+0xb4;firstend=o.entry+0xd8;second=o.entry+0xdc;secondend=o.entry+0xf4
  else:
   image=(out/'8261DC08.bin').read_bytes();c.mem.add(0x8261dc08,len(image),image);c.setg(28,R.M.S.AUX);c.setg(27,R.M.S.SELF);first=0x8261dd80;firstend=0x8261dda8;second=0x8261ddac;secondend=0x8261ddc4
  c.mem.wr(firstend,bytes.fromhex('4e800020'));c.run(first);c.setg(3,R.M.S.SELF+0x200);c.mem.wr(secondend,bytes.fromhex('4e800020'));c.run(second);results.append((c.g(3),c.g(4),c.vr[1],c.vr[2],tuple(c.fbits[1:4]),c.mem.rd(R.M.S.AUX,64)))
 if results[0]!=results[1]:errors.append({'case':i,'native':str(results[0][:-1]),'compiled':str(results[1][:-1])})
d={'cases':256,'mismatch_count':len(errors),'mismatches':errors[:5],'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'original 8261DD80..DDA4 and DDAC..DDC0 vs actual ProcessAddAirRamEvent pre/post lookup slices; 256 random raw 64-byte event payloads including arbitrary v1/v2 W and float bits, lookup result controlled; call-boundary r3/r4,v1/v2,f1-f3 and event bytes compared; prior index/gate, lookup and callee excluded'};(out/'traffic-airram-boundary-comparison.json').write_text(json.dumps(d,indent=2));print(json.dumps(d,indent=2));assert not errors
