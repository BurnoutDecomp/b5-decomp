from pathlib import Path
import sys,random,struct,json,hashlib
sys.path.insert(0,'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/scratch/x360_sdk');import validate_render_states as R
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave8/racecar_consumers');obj=out/'TrafficPhysics_Construct/probe.obj';o=R.Coff(obj,'?SetCrashingFromCheck@TrafficPhysics@');rng=random.Random(0x8261e);errors=[]
for i in range(256):
 data=bytearray(rng.randbytes(0x100));vel=[rng.uniform(-50,50) for _ in range(4)];direction=[rng.uniform(-1,1) for _ in range(4)];speed=struct.unpack('>f',struct.pack('>f',rng.uniform(0.0,50.0)))[0];minimum=struct.unpack('>I',struct.pack('>f',speed))[0];doubled=struct.unpack('>I',struct.pack('>f',speed*2))[0];d=struct.unpack('>4I',struct.pack('>4f',*direction));struct.pack_into('>4f',data,0x50,*vel);results=[]
 for compiled in (False,True):
  c=o.cpu() if compiled else R.M.new_cpu();c.mem.wr(R.M.S.SELF,data);c.setg(3,R.M.S.SELF)
  if compiled:
   calls=[]
   for addr,name in o.unresolved.items():
    if 'SetCrashing@VehiclePhysics' in name:c.hooks[addr]=lambda cpu:calls.append(cpu.g(3))
   c.vr[1]=d;c.vr[2]=(minimum,)*4;c.run(o.entry);assert calls==[R.M.S.SELF],calls
  else:
   image=(out/'8261E360.bin').read_bytes();c.mem.add(0x8261e360,len(image),image);c.mem.wr(0x8261e480,bytes.fromhex('4e800020'));c.vr[127]=d;c.vr[126]=(doubled,)*4;c.run(0x8261e44c)
  results.append(c.mem.rd(R.M.S.SELF,0x100))
 if results[0]!=results[1]:errors.append({'case':i,'offsets':[j for j,(a,b) in enumerate(zip(*results)) if a!=b][:16]})
d={'cases':256,'mismatch_count':len(errors),'mismatches':errors[:5],'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'original inlined OnChecked 8261E44C..E47C vs actual native TrafficPhysics::SetCrashingFromCheck provider; random finite raw direction/velocity, binary32 min-speed input, full poisoned 0x100 body; SetCrashing provider controlled and original caller after-call excluded, software VMX model, no hardware/game claim'};(out/'traffic-checked-velocity-comparison.json').write_text(json.dumps(d,indent=2));print(json.dumps(d,indent=2));assert not errors
