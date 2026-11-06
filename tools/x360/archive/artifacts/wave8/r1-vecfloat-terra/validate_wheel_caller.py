from pathlib import Path
import sys,random,struct,json,hashlib
sys.path.insert(0,'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/scratch/x360_sdk');import validate_render_states as R
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave8/r1-vecfloat-terra');obj=out/'VehiclePhysics/probe.obj';o=R.Coff(obj,'?UpdateWheels@VehiclePhysics@');rng=random.Random(0x8261f068);errors=[]
for i in range(256):
 lanes=[rng.randbytes(16) for _ in range(5)];dt,maximum,rear,cap,decel=lanes;flag1=i&1;flag2=(i>>1)&1;results=[]
 for compiled in (False,True):
  c=o.cpu() if compiled else R.M.new_cpu();sp=c.g(1);c.mem.wr(R.M.S.AUX,maximum);c.setg(3,R.M.S.AUX);c.vr[127]=struct.unpack('>4I',dt)
  if compiled:
   c.setg(31,R.M.S.SELF);c.setg(26,flag1);c.setg(25,flag2);c.mem.wr(sp+0xd0,decel);c.mem.wr(sp+0xe0,cap);c.mem.wr(sp+0x110,rear);start=o.entry+0x5ac;end=o.entry+0x5dc
  else:
   image=(out/'8261E4F0.bin').read_bytes();c.mem.add(0x8261e4f0,len(image),image);c.setg(17,R.M.S.SELF+0x3d0);c.setg(23,flag1);c.setg(22,flag2);c.setg(29,R.M.S.AUX+0x20);c.setg(28,R.M.S.AUX+0x40);c.mem.wr(R.M.S.AUX+0x20,decel);c.mem.wr(R.M.S.AUX+0x40,cap);c.vr[126]=struct.unpack('>4I',rear);start=0x8261f068;end=0x8261f08c
  c.mem.wr(end,bytes.fromhex('4e800020'));c.run(start);results.append((c.g(3),c.g(4),c.g(5),tuple(c.vr[j] for j in range(1,6))))
 if results[0]!=results[1]:errors.append([i,str(results[0]),str(results[1])])
d={'cases':256,'mismatch_count':len(errors),'mismatches':errors[:3],'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'actual UpdateWheels rear-right post-Engine-getter to Wheel::UpdateVelocity call boundary vs ARTIST 8261F068..F088, 256 arbitrary raw v1-v5 lanes and bool registers, getter result and wheel object controlled; actual Engine getter sret separately tested, no Wheel provider/hardware/game claim'};(out/'wheel-caller-boundary-comparison.json').write_text(json.dumps(d,indent=2));print(json.dumps(d,indent=2));assert not errors
