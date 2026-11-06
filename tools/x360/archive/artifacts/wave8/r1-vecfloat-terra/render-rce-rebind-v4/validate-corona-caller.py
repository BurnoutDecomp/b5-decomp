from pathlib import Path
import sys,random,struct,json,hashlib
sys.path.insert(0,'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/scratch/x360_sdk');import validate_render_states as R
base=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave8/r1-vecfloat-terra');out=base/'render-rce-rebind-v4';obj=out/'render/probe.obj';o=R.Coff(obj,'?SubmitCoronasForRaceCar@RaceCarEntityModule@BrnWorld@@');rng=random.Random(0x822d19e8);errors=[]
for i in range(256):
 pos=tuple(rng.getrandbits(32) for _ in range(4));direction=tuple(rng.getrandbits(32) for _ in range(4));opacity=[0.,.25,.5,1.,-1.,2.][i%6];enum=i%25;results=[]
 for compiled in (False,True):
  c=o.cpu() if compiled else R.M.new_cpu();sp=c.g(1);c.setf(31,opacity)
  if compiled:
   c.vr[1]=pos;c.vr[127]=direction;c.setg(25,R.M.S.SELF);c.setg(14,sp+0x6c);c.mem.wr(sp+0x6c,struct.pack('>I',enum));c.setf(30,1.0);c.mem.wr(o.entry+0x3d0,bytes.fromhex('4e800020'));c.run(o.entry+0x3bc)
  else:
   raw=b''.join(struct.pack('>I',R.M.V.x360rd.u32(x)) for x in range(0x822d19d4,0x822d19ec,4));c.mem.add(0x822d19d4,len(raw),raw);c.mem.add(0x82001000,0x1000);c.mem.wr(0x82001c98,struct.pack('>f',1.));c.setg(30,0x82000000);c.setg(29,R.M.S.SELF);c.vr[122]=pos;c.vr[2]=direction;c.mem.wr(sp+0x64,struct.pack('>I',enum));c.mem.wr(0x822d19e8,bytes.fromhex('4e800020'));c.run(0x822d19d4)
  results.append((c.g(3),c.vr[1],c.vr[2],c.f(1),c.f(2),c.mem.rd(c.g(6),4)))
 if results[0]!=results[1]:errors.append([i,str(results)])
d={'cases':256,'count':len(errors),'mismatches':errors[:2],'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'compiled_slice':'SubmitCoronasForRaceCar COMDAT+3BC..3CC','original_slice':'822D19D4..19E4','scope':'actual Render emitted first strobe AddCorona call boundary versus ARTIST with prior point transform and rearward vector controlled: r3, enum reference contents, v1/v2 all lanes, f1/f2. Provider/body and full earlier loop excluded.'};(out/'actual-corona-caller-comparison.json').write_text(json.dumps(d,indent=2)+'\n');print(json.dumps(d,indent=2));assert not errors
