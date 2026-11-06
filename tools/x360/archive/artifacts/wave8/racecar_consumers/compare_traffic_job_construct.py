from pathlib import Path
import sys,random,struct,json,hashlib
sys.path.insert(0,'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/scratch/x360_sdk');import validate_render_states as R
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave8/racecar_consumers');obj=out/'TrafficCommon/probe.obj';o=R.Coff(obj,'?Construct@UpdateVehiclesJobParams@');rng=random.Random(0x82706e28);errors=[]
for i in range(256):
 before=rng.randbytes(0x100);randomdata=rng.randbytes(0x30);v=tuple(rng.getrandbits(32) for _ in range(4));results=[]
 for compiled in (False,True):
  c=o.cpu() if compiled else R.M.new_cpu();sp=c.g(1);c.mem.wr(R.M.S.SELF,before);c.mem.wr(R.M.S.AUX,randomdata);c.setg(3,R.M.S.SELF)
  for reg in range(4,11):c.setg(reg,R.M.S.AUX)
  for off in range(0x54,0xd0,8):c.mem.wr(sp+off,struct.pack('>I',R.M.S.AUX))
  c.vr[1]=v
  for f in (1,2,3):c.fbits[f]=struct.unpack('>Q',struct.pack('>d',float(f)+i/256))[0]
  c.run(o.entry if compiled else 0x82706e28);results.append((c.mem.rd(R.M.S.SELF,0x100),c.mem.rd(R.M.S.AUX,0x30)))
 if results[0]!=results[1]:errors.append({'case':i,'offsets':[j for j,(a,b) in enumerate(zip(results[0][0],results[1][0])) if a!=b]})
d={'cases':256,'mismatch_count':len(errors),'mismatches':errors[:5],'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'full actual UpdateVehiclesJobParams::Construct vs ARTIST 82706E28 with non-null inputs, random 16-byte vector including arbitrary raw W, copied random state, stack args and poisoned 0x100 destination; assertions not entered, software VMX no full job claim'};(out/'traffic-job-construct-comparison.json').write_text(json.dumps(d,indent=2));print(json.dumps(d,indent=2));assert not errors
