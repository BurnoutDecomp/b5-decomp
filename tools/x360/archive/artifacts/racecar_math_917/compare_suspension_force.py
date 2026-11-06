import sys,struct,random,json,hashlib
from pathlib import Path
sys.path.insert(0,'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/scratch/x360_sdk');import validate_render_states as R
base=Path('D:/Reverse/BP-X360-Experimental-Artifacts/racecar_math_917');p=base/'airram_provider/probe.obj';o=R.Coff(p,'?SuspensionForce@');rng=random.Random(91711);errors=[]
for i in range(256):
 mag=struct.unpack('>f',struct.pack('>f',rng.uniform(0.001,10000)))[0];values=tuple(rng.uniform(-3,3) for _ in range(4));u=struct.unpack('>4I',struct.pack('>4f',*values));results=[]
 for compiled in (False,True):
  c=o.cpu() if compiled else R.M.new_cpu();c.setf(1,mag)
  if not compiled:
   data=(base/'825D1EE8.bin').read_bytes();c.mem.add(0x825d1ee8,len(data),data);c.mem.wr(0x825d2028,bytes.fromhex('4e800020'));sp=c.g(1);c.mem.wr(sp+0x90,struct.pack('>f',mag));c.setg(11,sp+0x94);c.setg(10,sp+0xb0);c.setg(30,0);c.vr[13]=u;c.vr[11]=(0x3f000000,)*4;c.vr[12]=(0x3f800000,)*4
  else:c.vr[1]=u
  c.run(o.entry if compiled else 0x825d1fd4);results.append(c.vr[1] if compiled else c.vr[127])
 if results[0]!=results[1]:errors.append({'case':i,'native':results[0],'compiled':results[1]})
d={'cases':256,'mismatches':errors[:10],'mismatch_count':len(errors),'object_sha256':hashlib.sha256(p.read_bytes()).hexdigest(),'scope':'original 825D1FD4..2024 vs actual SuspensionForce object, random finite vector/magnitude, existing input register/stack seeded, software estimate model; no original surrounding caller, physical hardware or gameplay claim'};(base/'suspension-force-comparison.json').write_text(json.dumps(d,indent=2));print(json.dumps(d,indent=2));assert not errors
