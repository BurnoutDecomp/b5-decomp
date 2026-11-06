from pathlib import Path
import sys,random,struct,json,hashlib
sys.path.insert(0,'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/scratch/x360_sdk');import validate_render_states as R
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave8/racecar_consumers');obj=out/'BrnEffectsUtils/probe.obj';rng=random.Random(0x82277e);errors=[]
for symbol,entry in [('?RandomiseXYZ@Vector3Randomiser@',0x82277ec8),('?RandomiseXYZW@Vector4Randomiser@',0x82277fb8)]:
 o=R.Coff(obj,symbol)
 for i in range(256):
  v=[rng.uniform(-50,50) for _ in range(8)];selfdata=struct.pack('>8f',*v);rd=bytearray(rng.randbytes(0x100));struct.pack_into('>8I',rd,0,*[0x3f800000|rng.getrandbits(23) for _ in range(8)]);struct.pack_into('>I',rd,0x28,rng.randrange(8));results=[]
  for compiled in (False,True):
   c=o.cpu() if compiled else R.M.new_cpu();c.mem.wr(R.M.S.SELF,selfdata);c.mem.wr(R.M.S.AUX,rd);c.setg(3,(R.M.S.AUX+0x200));c.setg(4,R.M.S.SELF);c.setg(5,R.M.S.AUX);c.run(o.entry if compiled else entry);results.append((c.mem.rd((R.M.S.AUX+0x200),16),c.mem.rd(R.M.S.AUX,0x100)))
  if results[0]!=results[1]:errors.append({'symbol':symbol,'case':i,'out':[x[0].hex() for x in results]})
d={'cases_per_function':256,'mismatch_count':len(errors),'mismatches':errors[:5],'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'full actual RandomiseXYZ and RandomiseXYZW versus original ARTIST, finite bounds and valid primed random rings, random seed/index, output and full random-state bytes; software VMX, no runtime or GPU claim'};(out/'effects-randomiser-comparison.json').write_text(json.dumps(d,indent=2));print(json.dumps(d,indent=2));assert not errors

