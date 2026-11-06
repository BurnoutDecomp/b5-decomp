from pathlib import Path
import sys,random,struct,json,hashlib
sys.path.insert(0,'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/scratch/x360_sdk');import validate_render_states as R
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave8/racecar_consumers');obj=out/'BrnPhysicalTrafficManager/probe.obj';o=R.Coff(obj,'?ValidateTrafficContact@PhysicalTrafficManager@');rng=random.Random(0x825ca);errors=[]
for i in range(256):
 def vec():return struct.pack('>4f',*[rng.uniform(-2,2) for _ in range(4)])
 body=bytearray(rng.randbytes(0x800));up=vec();normal=vec();ground=vec();contact=vec();body[0x20:0x30]=up;body[0x570:0x580]=ground;body[0x580:0x590]=normal
 if i<8:
  edges=[0,0x80000000,1,0x80000001,0x7f800000,0xff800000,0x7f800001,0x7fc00001];struct.pack_into('>I',body,0x20,edges[i]);up=bytes(body[0x20:0x30])
 valid=i&1;results=[]
 for compiled in (False,True):
  c=o.cpu() if compiled else R.M.new_cpu();sp=c.g(1);c.mem.wr(R.M.S.SELF,body);c.mem.wr(R.M.S.AUX,contact);c.mem.wr(R.M.S.AUX+0x100,struct.pack('>4f',*([0.4]*4)));c.setg(31,R.M.S.SELF);c.setg(30,R.M.S.AUX);c.setg(27,0)
  if compiled:c.setg(3,valid);c.mem.wr(sp+0x70,struct.pack('>4f',*([0.4]*4)));start=o.entry+0x16c;end=o.entry+0x1f0
  else:
   image=(out/'825CACB8.bin').read_bytes();c.mem.add(0x825cacb8,len(image),image);c.setg(28,valid);c.setg(29,R.M.S.AUX+0x100);c.vr[127]=struct.unpack('>4I',up);start=0x825caeb8;end=0x825caf38
  c.mem.wr(end,bytes.fromhex('4e800020'));c.run(start);results.append((c.g(3)&255 if compiled else c.g(28)&255,c.mem.rd(R.M.S.SELF,0x800),c.mem.rd(R.M.S.AUX,16)))
 if results[0]!=results[1]:errors.append({'case':i,'native':results[0][0],'compiled':results[1][0]})
d={'cases':256,'mismatch_count':len(errors),'mismatches':errors[:8],'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'actual traffic contact upright/height VMX decision slice vs ARTIST 825CAEB8..CAF34; random finite vectors plus signed-zero/subnormal/infinite/NaN lanes and initial validity, full body/contact inputs checked, threshold seeded 0.4; prior wheel-contact path and asserts controlled, no hardware claim'};(out/'traffic-contact-vmx-comparison.json').write_text(json.dumps(d,indent=2));print(json.dumps(d,indent=2));assert not errors

