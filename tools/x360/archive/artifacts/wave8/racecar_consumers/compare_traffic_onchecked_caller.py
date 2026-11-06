from pathlib import Path
import sys,random,struct,json,hashlib
sys.path.insert(0,'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/scratch/x360_sdk');import validate_render_states as R
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave8/racecar_consumers');obj=out/'BrnPhysicalTrafficManager/probe.obj';o=R.Coff(obj,'?OnChecked@PhysicalTrafficVehicle@');rng=random.Random(0x8261e44c);errors=[]
for i in range(256):
 v=tuple(rng.getrandbits(32) for _ in range(4)); results=[]
 for compiled in (False,True):
  c=o.cpu() if compiled else R.M.new_cpu()
  if compiled:
   c.setg(28,R.M.S.SELF);c.setg(29,R.M.S.AUX);c.mem.wr(R.M.S.SELF+0x1340,struct.pack('>4I',*v))
   for addr,name in o.unresolved.items():
    if 'GetFullTrafficPhysics@PhysicalTrafficVehicle' in name:c.hooks[addr]=lambda cpu:cpu.setg(3,R.M.S.AUX)
   start=o.entry+0xd0;end=o.entry+0xf8
  else:
   image=(out/'8261E360.bin').read_bytes();c.mem.add(0x8261e360,len(image),image);c.setg(3,R.M.S.SELF);c.vr[126]=(v[3],)*4;c.vr[127]=v;start=0x8261e44c;end=0x8261e460
  c.mem.wr(end,bytes.fromhex('4e800020'));c.run(start);results.append((c.vr[1],c.vr[2]) if compiled else (c.vr[127],c.vr[13]))
 if results[0]!=results[1]:errors.append({'case':i,'source':v,'original':results[0],'compiled':results[1]})
d={'cases':256,'mismatch_count':len(errors),'mismatches':errors[:5],'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'original OnChecked 8261E44C..E45C versus actual OnChecked VMX call arguments; raw random all four lanes including arbitrary NaNs, original v126 seeded from attested splat and v127 direction, getter controlled; software VMX model, downstream provider separately compared; no hardware claim'};(out/'traffic-onchecked-vmx-caller-comparison.json').write_text(json.dumps(d,indent=2));print(json.dumps(d,indent=2));assert not errors

