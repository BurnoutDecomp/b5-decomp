import sys,struct,random,json,hashlib
from pathlib import Path
sys.path.insert(0,'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/scratch/x360_sdk');import validate_render_states as R
base=Path('D:/Reverse/BP-X360-Experimental-Artifacts/racecar_math_combined');p=base/'airram_provider/probe.obj';rng=random.Random(91752);errors=[];specs=[('KF_SHOWTIME_GRIP_SCALE',0x82c5c3c0,0x82fb9ec0),('KF_SHOWTIME_PLAYER_ANGULAR_IMPULSE_SCALE',0x82c5c3e8,0x82fb8b10),('gvMaxCrashAngularVelocity',0x82c5c5b8,0x82fb9da0)]
for name,origin,native_dest in specs:
 o=R.Coff(p,'??__E'+name+'@');o.images[0x83800000].extend(b'\0'*16);item=[x for x in o.symbols.values() if x[0].startswith('?'+name+'@')][0];compiled_dest=0x83800000+item[1]
 for case in range(64):
  poison=rng.randbytes(48);values=[]
  for compiled in (False,True):
   c=o.cpu() if compiled else R.M.new_cpu();dest=compiled_dest if compiled else native_dest
   if not compiled:c.mem.add(dest-16,48)
   c.mem.wr(dest-16,poison);c.run(o.entry if compiled else origin);value=c.mem.rd(dest-16,48);assert value[:16]==poison[:16] and value[32:]==poison[32:];values.append(value[16:32])
  if values[0]!=values[1]:errors.append({'name':name,'case':case,'native':values[0].hex(),'compiled':values[1].hex()})
d={'cases_per_initializer':64,'initializers':[x[0] for x in specs],'mismatches':errors,'object_sha256':hashlib.sha256(p.read_bytes()).hexdigest(),'scope':'whole native CRT initializers vs actual object with poisoned globals and adjacent guards; original constant inputs; lifecycle ordering and hardware not tested'};(base/'vehicle-globals-crt-comparison.json').write_text(json.dumps(d,indent=2));print(json.dumps(d,indent=2));assert not errors
