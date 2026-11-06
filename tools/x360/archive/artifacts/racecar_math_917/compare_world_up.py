import sys,random,json,hashlib
from pathlib import Path
sys.path.insert(0,'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/scratch/x360_sdk');import validate_render_states as R
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/racecar_math_917');obj=out/'physics/probe.obj';coff=R.Coff(obj,'??__EKV_AFTERTOUCH_WORLD_UP@');rng=random.Random(360);errors=[]
for i in range(64):
 poison=rng.randbytes(16); results=[]
 for compiled in (False,True):
  c=coff.cpu() if compiled else R.M.new_cpu();dest=0x83830000 if compiled else 0x82fb9050
  if not compiled: c.mem.add(dest,16)
  before=rng.randbytes(16);after=rng.randbytes(16);c.mem.add(dest-16,16,before);c.mem.add(dest+16,16,after)
  c.mem.wr(dest,poison);c.run(coff.entry if compiled else 0x82c5c4b0);results.append(c.mem.rd(dest,16));assert c.mem.rd(dest-16,16)==before and c.mem.rd(dest+16,16)==after
 if results[0]!=results[1]:errors.append((i,[x.hex() for x in results]))
d={'cases':64,'mismatches':errors,'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'original':'82C5C4B0','compiled_symbol':coff.fn[0],'scope':'whole CRT initializer, 16 destination bytes after randomized poisoning; real constants from native image, no external calls; adjacent 16-byte guards checked; lifecycle ordering not tested'};(out/'world-up-crt-comparison.json').write_text(json.dumps(d,indent=2));print(d);assert not errors

