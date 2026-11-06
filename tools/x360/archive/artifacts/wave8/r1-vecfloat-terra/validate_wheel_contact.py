from pathlib import Path
import sys,random,struct,json,hashlib
sys.path.insert(0,'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/scratch/x360_sdk');import validate_render_states as R
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave8/r1-vecfloat-terra');obj=out/'Wheel/probe.obj';o=R.Coff(obj,'?SetRoadContact@Wheel@');rng=random.Random(0x825d6c08);errors=[]
for i in range(256):
 initial=rng.randbytes(0x120);pos=struct.unpack('>4I',struct.pack('>4f',*[rng.uniform(-100,100) for _ in range(4)]));normal=struct.unpack('>4I',struct.pack('>4f',*[rng.uniform(-1,1) for _ in range(4)]));bits=struct.unpack('>Q',struct.pack('>d',rng.uniform(0,30)))[0];on=i&1;near=(i>>1)&1;hi=rng.randrange(65536);lo=rng.randrange(65536);results=[]
 for compiled in (False,True):
  c=o.cpu() if compiled else R.M.new_cpu();c.mem.wr(R.M.S.SELF,initial);c.setg(3,R.M.S.SELF);c.setg(4,on);c.setg(5,near);c.setg(6,hi);c.setg(7,lo);c.vr[1]=pos;c.vr[2]=normal;c.fbits[1]=bits
  if not compiled:
   image=(out/'825D6C08.bin').read_bytes();c.mem.add(0x825d6c08,len(image),image)
  c.run(o.entry if compiled else 0x825d6c08);results.append(c.mem.rd(R.M.S.SELF,0x120))
 if results[0]!=results[1]:errors.append({'case':i,'offsets':[j for j,(a,b) in enumerate(zip(*results)) if a!=b]})
d={'cases':256,'mismatch_count':len(errors),'mismatches':errors[:5],'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'whole actual Wheel::SetRoadContact vs ARTIST 825D6C08, finite random four-lane contact vectors, scalar distance, flags and tags; poisoned full object checked, assert paths excluded, software VMX'};(out/'wheel-contact-comparison.json').write_text(json.dumps(d,indent=2));print(json.dumps(d,indent=2));assert not errors
