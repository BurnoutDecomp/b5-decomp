from pathlib import Path
import sys,random,struct,json,hashlib
sys.path.insert(0,'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/scratch/x360_sdk');import validate_render_states as R
base=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave8/r1-vecfloat-terra');out=base/'render-rce-rebind-v2';obj=out/'render.obj';o=R.Coff(obj,'?RaceCarDistance@BrnWorld@@');rng=random.Random(0x822e7c3c);errors=[]
for i in range(512):
 cam=struct.unpack('>4I',struct.pack('>4f',*[rng.uniform(-500,500) for _ in range(4)]));pos=cam if i%16==0 else struct.unpack('>4I',struct.pack('>4f',*[rng.uniform(-500,500) for _ in range(4)]));results=[]
 for compiled in (False,True):
  c=o.cpu() if compiled else R.M.new_cpu()
  if compiled:
   c.vr[1]=cam;c.vr[2]=pos;c.setg(3,R.M.S.AUX);c.run(o.entry);results.append((c.vr[1],struct.unpack('>4I',c.mem.rd(R.M.S.AUX,16))))
  else:
   image=(base/'822E79F8.bin').read_bytes();c.mem.add(0x822e79f8,len(image),image);c.mem.wr(R.M.S.SELF+0x30,struct.pack('>4I',*pos));c.setg(3,R.M.S.SELF);c.setg(26,0x30);c.vr[123]=cam;c.mem.wr(0x822e7c94,bytes.fromhex('4e800020'));c.run(0x822e7c3c);results.append((c.vr[127],c.vr[0]))
 if results[0]!=results[1]:errors.append([i,results[0],results[1]])
d={'cases':512,'mismatches':errors[:5],'count':len(errors),'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'symbol':'?RaceCarDistance@BrnWorld@@','original_slice':'822E7C3C..822E7C90','scope':'actual Render CPP emitted helper vs ARTIST distance instruction slice, finite camera/position vectors incl equal points; all lanes and squared output; modeled reciprocal estimate, no cull/full render/runtime claim'};(out/'actual-distance-comparison.json').write_text(json.dumps(d,indent=2)+'\n');print(json.dumps(d,indent=2));assert not errors
