import sys,struct,random,json,hashlib
from pathlib import Path
sys.path.insert(0,'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/scratch/x360_sdk');import validate_render_states as R
base=Path('D:/Reverse/BP-X360-Experimental-Artifacts/racecar_math_combined');p=base/'airram_provider/probe.obj';o=R.Coff(p,'?TakeoffReciprocal@');rng=random.Random(91710);errors=[]
for i,u in enumerate([0,0x80000000,1,0x80000001,0x007fffff,0x00800000,0x7f800000,0xff800000,0x7fc00000,0xbf800000]):
 x=struct.unpack('>f',struct.pack('>I',u))[0];results=[]
 for compiled in (False,True):
  c=o.cpu() if compiled else R.M.new_cpu()
  if not compiled:
   data=(base/'825D0BE8.bin').read_bytes();c.mem.add(0x825d0be8,len(data),data);c.mem.wr(0x825d0d0c,bytes.fromhex('4e800020'));c.vr[0]=c.vr[9]=c.vr[10]=(u,)*4;c.vr[12]=(1,)*4
  else:c.setf(1,x)
  c.run(o.entry if compiled else 0x825d0ce0);results.append(struct.unpack('>I',struct.pack('>f',c.f(1)))[0] if compiled else c.vr[0][0])
 if results[0]!=results[1]:errors.append({'case':i,'input':hex(u),'original':hex(results[0]),'compiled':hex(results[1])})
d={'cases':10,'mismatches':errors[:20],'mismatch_count':len(errors),'object_sha256':hashlib.sha256(p.read_bytes()).hexdigest(),'scope':'original 825D0CE0..0D08 versus actual TakeoffReciprocal object, edge binary32 input and seeded existing vectors; VMX estimate software model, no hardware claim'};(base/'takeoff-reciprocal-specials-comparison.json').write_text(json.dumps(d,indent=2));print(json.dumps(d,indent=2));assert not errors
