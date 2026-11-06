from pathlib import Path
import sys,random,struct,json,hashlib
sys.path.insert(0,'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/scratch/x360_sdk');import validate_render_states as R
from validate_resource_pointer import Bundle
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave8/r1-vecfloat-terra');pa=out/'Engine/probe.obj';pb=out/'vecfloat/probe.obj';symbol='?GetMaxWheelAngularVelocity@Engine@';a=R.Coff(pa,symbol);b=R.Coff(pb,'??0VecFloat@vpu@math@rw@@QAA@ABM@');bundle=Bundle([a,b]);bundle.select(symbol);rng=random.Random(0x825bfda0);errors=[]
for i in range(256):
 raw=rng.getrandbits(32);before=rng.randbytes(0x80);results=[]
 for compiled in (False,True):
  c=bundle.cpu() if compiled else R.M.new_cpu();target=R.M.S.SELF+16;c.mem.wr(R.M.S.SELF,before);sp=c.g(1)
  if compiled:c.setg(31,target);c.mem.wr(sp+0x50,struct.pack('>I',raw));start=bundle.entry+0x90;end=bundle.entry+0xac
  else:
   image=(out/'825BFDA0.bin').read_bytes();c.mem.add(0x825bfda0,len(image),image);c.setg(30,target);c.setg(3,target);c.vr[0]=(raw,)*4;start=0x825bfee0;end=0x825bfee4
  c.mem.wr(end,bytes.fromhex('4e800020'));c.run(start);results.append((c.g(3),c.mem.rd(R.M.S.SELF,0x80)))
 if results[0]!=results[1]:errors.append(i)
d={'cases':256,'mismatch_count':len(errors),'mismatches':errors[:5],'provider_object_sha256':hashlib.sha256(pa.read_bytes()).hexdigest(),'vecfloat_provider_sha256':hashlib.sha256(pb.read_bytes()).hexdigest(),'scope':'actual Engine::GetMaxWheelAngularVelocity sret construction/store and return pointer vs ARTIST 825BFEE0, 256 arbitrary raw 32-bit results and guard bytes; real SDK VecFloat constructor linked, math and zero/assert branches excluded, software VMX only'};(out/'engine-getter-sret-comparison.json').write_text(json.dumps(d,indent=2));print(json.dumps(d,indent=2));assert not errors
