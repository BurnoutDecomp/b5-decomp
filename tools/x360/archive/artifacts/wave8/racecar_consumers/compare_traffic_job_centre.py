from pathlib import Path
import sys,random,json,hashlib
sys.path.insert(0,'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/scratch/x360_sdk');import validate_render_states as R
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave8/racecar_consumers');obj=out/'BrnUpdateVehiclesJob/probe.obj';o=R.Coff(obj,'?UpdateVehicle@UpdateVehiclesJob@');rng=random.Random(0x8291d9ac);errors=[]
for i in range(256):
 centre=rng.randbytes(16);pos=rng.randbytes(16);results=[]
 for compiled in (False,True):
  c=o.cpu() if compiled else R.M.new_cpu();sp=c.g(1);c.mem.wr(R.M.S.AUX+0x80,centre)
  if compiled:
   c.mem.wr(sp+0x110,pos);c.setg(9,sp+0x110);c.setg(11,R.M.S.AUX);c.setg(10,0x80);start=o.entry+0x19c;end=o.entry+0x1b4;dst=sp+0xb0
  else:
   image=(out/'8291D810.bin').read_bytes();c.mem.add(0x8291d810,len(image),image);c.mem.wr(R.M.S.SELF+0x30,pos);c.setg(3,R.M.S.SELF);c.setg(10,0x30);c.setg(11,R.M.S.AUX);c.setg(9,0x80);start=0x8291d9b8;end=0x8291d9d0;dst=sp+0x80
  c.mem.wr(end,bytes.fromhex('4e800020'));c.run(start);results.append((c.vr[0],c.mem.rd(dst,16)))
 if results[0]!=results[1]:errors.append([i,results[0][0],results[1][0]])
d={'cases':256,'mismatch_count':len(errors),'mismatches':errors[:5],'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'actual UpdateVehicle centre vsubfp/vmsum3fp/store slice versus ARTIST 8291D9B8..D9CC; random raw all-lane centre and transform position including W, pointer and stack inputs seeded, following floating comparison and full job excluded; software VMX'};(out/'traffic-job-centre-comparison.json').write_text(json.dumps(d,indent=2));print(json.dumps(d,indent=2));assert not errors

