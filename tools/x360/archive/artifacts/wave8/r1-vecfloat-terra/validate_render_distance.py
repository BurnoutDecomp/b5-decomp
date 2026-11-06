from pathlib import Path
import sys,random,struct,json,hashlib
sys.path.insert(0,'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/scratch/x360_sdk');import validate_render_states as R
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave8/r1-vecfloat-terra');obj=out/'render_distance_fixture/probe.obj';o=R.Coff(obj,'?ProbeDistance@@');rng=random.Random(0x822e7c3c);errors=[]
for i in range(256):
 cam=struct.unpack('>4I',struct.pack('>4f',*[rng.uniform(-500,500) for _ in range(4)]));pos=cam if i%16==0 else struct.unpack('>4I',struct.pack('>4f',*[rng.uniform(-500,500) for _ in range(4)]));results=[]
 for compiled in (False,True):
  c=o.cpu() if compiled else R.M.new_cpu()
  if compiled:c.vr[1]=cam;c.vr[2]=pos;c.setg(3,R.M.S.AUX);c.run(o.entry);results.append((c.vr[1],struct.unpack('>4I',c.mem.rd(R.M.S.AUX,16))))
  else:
   image=(out/'822E79F8.bin').read_bytes();c.mem.add(0x822e79f8,len(image),image);c.mem.wr(R.M.S.SELF+0x30,struct.pack('>4I',*pos));c.setg(3,R.M.S.SELF);c.setg(26,0x30);c.vr[123]=cam;c.mem.wr(0x822e7c94,bytes.fromhex('4e800020'));c.run(0x822e7c3c);results.append((c.vr[127],c.vr[0]))
 if results[0]!=results[1]:errors.append([i,results[0],results[1]])
d={'cases':256,'mismatch_count':len(errors),'mismatches':errors[:3],'fixture_source_sha256':hashlib.sha256((out/'render-distance-fixture.cpp').read_bytes()).hexdigest(),'fixture_object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'isolated fixture copied exact current Render.cpp RaceCarDistance helper vs original 822E7C3C..7C90; 256 finite random camera/position vectors incl equal points, full four-lane distance and squared results, software reciprocal estimates; actual Render product CPP and cull calls excluded'};(out/'render-distance-fixture-comparison.json').write_text(json.dumps(d,indent=2));print(json.dumps(d,indent=2));assert not errors
