import sys,struct,random,json,hashlib
from pathlib import Path
sys.path.insert(0,'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/scratch/x360_sdk');import validate_render_states as R
base=Path('D:/Reverse/BP-X360-Experimental-Artifacts/racecar_math_combined');p=base/'airram_provider/probe.obj';specs=[('TakeoffGreaterEqual',0x825d0cc0,0x825d0ccc),('TakeoffGreater',0x825d0d20,0x825d0d2c)];rng=random.Random(217);edges=[0,0x80000000,1,0x80000001,0x7f800000,0xff800000,0x7fc00000,0x3f800000];errors=[]
for name,start,end in specs:
 o=R.Coff(p,'?'+name+'@')
 for i in range(256):
  ua=edges[i%len(edges)] if i<64 else struct.unpack('>I',struct.pack('>f',rng.uniform(-100,100)))[0];ub=edges[(i//len(edges))%len(edges)] if i<64 else struct.unpack('>I',struct.pack('>f',rng.uniform(-100,100)))[0];results=[]
  for compiled in (False,True):
   c=o.cpu() if compiled else R.M.new_cpu()
   if not compiled:
    image=(base/'825D0BE8.bin').read_bytes();c.mem.add(0x825d0be8,len(image),image);c.mem.wr(end,bytes.fromhex('4e800020'));c.vr[0]=((ua if name=='TakeoffGreaterEqual' else ub),)*4;c.vr[13]=((ub if name=='TakeoffGreaterEqual' else ua),)*4
   else:c.setf(1,struct.unpack('>f',struct.pack('>I',ua))[0]);c.setf(2,struct.unpack('>f',struct.pack('>I',ub))[0])
   c.run(o.entry if compiled else start);results.append(c.g(3)&255 if compiled else c.g(11)&1)
  if results[0]!=results[1]:errors.append({'fn':name,'case':i,'a':hex(ua),'b':hex(ub),'native':results[0],'compiled':results[1]})
d={'cases_per_function':256,'mismatch_count':len(errors),'mismatches':errors[:10],'object_sha256':hashlib.sha256(p.read_bytes()).hexdigest(),'scope':'actual published VMX ordered-comparison helpers vs original 825D0CC0 and 825D0D20, finite/signed-zero/subnormal/NaN inputs, native operand order; hardware not tested'};(base/'takeoff-ordered-comparison.json').write_text(json.dumps(d,indent=2));print(json.dumps(d,indent=2));assert not errors
