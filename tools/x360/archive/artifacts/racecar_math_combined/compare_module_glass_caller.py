import sys,struct,random,json,hashlib
from pathlib import Path
sys.path.insert(0,'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/scratch/x360_sdk');import validate_render_states as R
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/racecar_math_combined');obj=out/'module/probe.obj';o=R.Coff(obj,'?ReadUpdatedActiveRaceCarDataFromPhysics@RaceCarEntityModule@');rng=random.Random(654);errors=[]
for i in range(128):
 width=struct.unpack('>f',struct.pack('>f',rng.uniform(0.0001,100)))[0];pane=i%8;poison=rng.randbytes(16);results=[]
 for compiled in (False,True):
  c=o.cpu() if compiled else R.M.new_cpu();sp=c.g(1)
  if compiled:
   c.setg(29,R.M.S.SELF);c.setg(30,pane);c.setf(31,width);dest=sp+0x58;start=o.entry+0x490;end=o.entry+0x4a4
  else:
   image=(out/'822E87B8.bin').read_bytes();c.mem.add(0x822e87b8,len(image),image);c.setg(30,R.M.S.SELF);c.setg(31,pane);c.mem.wr(sp+0xd0,struct.pack('>f',width));dest=sp+0xb0;start=0x822e8d28;end=0x822e8d40
  c.mem.wr(dest,poison);c.mem.wr(end,bytes.fromhex('4e800020'));c.run(start);assert c.g(5)==dest;buf=c.mem.rd(dest,16);assert buf[8:]==poison[8:];results.append((c.g(3),c.g(4),buf[:8]))
 if results[0]!=results[1]:errors.append({'case':i,'native':str(results[0]),'compiled':str(results[1])})
d={'cases':128,'mismatch_count':len(errors),'mismatches':errors[:5],'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'original smashed-glass 822E8D28..3C vs actual compiled caller +490..4A0 to FPU8 setter boundary; random finite width, valid panes, r3/r4 and 8 argument bytes, adjacent 8-byte poison guard; prior path and setter controlled, cracked inline branch and whole function excluded'};(out/'module-glass-caller-comparison.json').write_text(json.dumps(d,indent=2));print(json.dumps(d,indent=2));assert not errors
